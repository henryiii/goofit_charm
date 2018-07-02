// ROOT stuff
#include <TApplication.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TGraph.h>
#include <TLegend.h>
#include <TMath.h>
#include <TRandom.h>
#include <TRandom3.h>
#include <TTree.h>
#include <TROOT.h>
#include <TMinuit.h>
#include <TNtuple.h>
#include <TComplex.h>


// System stuff
#include <CLI/Timer.hpp>
#include <fstream>
#include <string>

// GooFit stuff
#include <goofit/Application.h>
#include <goofit/BinnedDataSet.h>
#include <goofit/FitManager.h>
#include <goofit/fitting/FitManagerMinuit2.h>
#include <goofit/PDFs/GooPdf.h>
#include <goofit/PDFs/basic/PolynomialPdf.h>
#include <goofit/PDFs/basic/SmoothHistogramPdf.h>
#include <goofit/PDFs/combine/AddPdf.h>
#include <goofit/PDFs/combine/ProdPdf.h>
#include <goofit/PDFs/physics/DalitzPlotPdf.h>
#include <goofit/PDFs/physics/DalitzVetoPdf.h>
#include <goofit/PDFs/physics/ResonancePdf.h>
#include <goofit/UnbinnedDataSet.h>
#include <goofit/Variable.h>
#include <goofit/PDFs/physics/DalitzPlotter.h>


#include <thrust/transform_reduce.h>

using namespace std;
using namespace GooFit;
using namespace ROOT;


//Globals

double pi_MASS  = 0.13957018; //GEV
double D_MASS   = 1.86959; //GEV

double d1_MASS  = pi_MASS;  //daughter 1 mass
double d2_MASS  = pi_MASS;
double d3_MASS  = pi_MASS;


fptype s12_min = POW2(d1_MASS  + d2_MASS);
fptype s12_max = POW2(D_MASS   - d2_MASS);
fptype s13_min = POW2(d1_MASS  + d3_MASS);
fptype s13_max = POW2(D_MASS   - d2_MASS);

Observable s12("s12",s12_min,s12_max); //s12^{2}
Observable s13("s13",s13_min,s13_max);
EventNumber eventNumber("eventNumber");

DalitzPlotPdf* signaldalitz = nullptr;
UnbinnedDataSet* Data = nullptr;

std::vector<PdfBase *> comps;
vector<fptype> HH_bin_limits;
vector<Variable> pwa_coefs_amp;
vector<Variable> pwa_coefs_phs;

Variable massSum("massSum", POW2(D_MASS) + POW2(d1_MASS) + POW2(d2_MASS) + POW2(d3_MASS));


// PWA INPUT FILE NAME
const string pwa_file = "files/PWACOEFS.txt";

//functions
fptype cpuGetM23(fptype massPZ, fptype massPM) { return (massSum.getValue() - massPZ - massPM); }

DalitzPlotPdf *makesignalpdf(GooPdf *eff = 0);

void maketoydalitzdata(GooPdf* overallsignal,std::string name, size_t nEvents){

DalitzPlotter dp(overallsignal,signaldalitz);

Data = new UnbinnedDataSet({s12,s13,eventNumber});

    std::cout << "Toy Generation begin!" << '\n';
    {
        TCanvas foo;
        auto th1 = dp.make2D();
        th1->Rebin2D(5,5);
        th1->GetXaxis()->SetTitle("#pi^{-}#pi^{+} [Gev/c^{2}]");
        th1->GetYaxis()->SetTitle("#pi^{-}#pi^{+} [Gev/c^{2}]");
        th1->SetStats(0);
        th1->Draw("COLZ");
        foo.SaveAs("plots/PDF.png");
        std::cout << "PDF plotted" << '\n';
    }

        dp.fillDataSetMC(*Data,nEvents);
        TH2F th2("toyData", "", 200, s12.getLowerLimit(), s12.getUpperLimit(), 200, s13.getLowerLimit(),
                         s13.getUpperLimit());
    th2.GetXaxis()->SetTitle("#pi^{-}#pi^{+} [Gev/c^{2}]");
    th2.GetYaxis()->SetTitle("#pi^{-}#pi^{+} [Gev/c^{2}]");

    {
        ofstream w(name);

            for (size_t i = 0; i < Data->getNumEvents(); i++) {
                Data->loadEvent(i);
                th2.Fill(s12, s13);
                w << i << "\t" << std::setprecision(6) << s12.getValue() << "\t" << s13.getValue() << '\n';

            }

            std::cout << "nEvents generated = " << Data->getNumEvents() << '\n';

        w.close();

    }

    TCanvas foo;
    th2.Draw("COLZ");
    th2.SetStats(0);
    foo.SaveAs("plots/toyData.png");

    std::cout << "toy data plotted" << '\n';
    std::cout << "toy Generation end!" << '\n';

}

ResonancePdf *loadPWAResonance(const string fname = pwa_file, bool fixAmp = false) {

    std::ifstream reader;
	//GOOFIT_INFO("LOADING FILE {}",fname);
    reader.open(fname.c_str());
    assert(reader.good());
    HH_bin_limits.clear();
    pwa_coefs_amp.clear();
    pwa_coefs_phs.clear();

    double e1, e2, e3;
    double emag, ephs;
    int i = 0;
    while(reader >> e1 >> e2 >> e3) {

        HH_bin_limits.push_back(e1);

        emag = sqrt(e2*e2 + e3*e3);
        ephs = TMath::ATan2(e3, e2);

        Variable va(fmt::format("pwa_coef_{}_mag", i), emag, .000001, -100.0, +100.0);
        Variable vp(fmt::format("pwa_coef_{}_phase", i), ephs, .000001, -360.0, +360.0);

        pwa_coefs_amp.push_back(va);
        pwa_coefs_phs.push_back(vp);
        i++;

    }

    Variable swave_amp_real("swave_amp_real", 1.0, 0.001, -50.0,+50.0);
    Variable swave_amp_imag("swave_amp_imag", 0.0, 0.001, -50.0,+50.0);

    if(fixAmp) {
        swave_amp_real.setValue(1.);
        swave_amp_imag.setValue(0.);
        swave_amp_real.setFixed(true);
        swave_amp_imag.setFixed(true);
    }
    cout << "Numbers loaded: " << HH_bin_limits.size() << " / " << i << endl;

    ResonancePdf *swave_12 = new Resonances::Spline("swave_12", swave_amp_real, swave_amp_imag, HH_bin_limits, pwa_coefs_amp, pwa_coefs_phs, PAIR_12, false);

    return swave_12;
} 

DalitzPlotPdf* makesignalpdf(GooPdf* eff){

    DecayInfo3 dtoppp;
    dtoppp.motherMass   = D_MASS;
    dtoppp.daug1Mass    = d1_MASS;
    dtoppp.daug2Mass    = d2_MASS;
    dtoppp.daug3Mass    = d3_MASS;
    dtoppp.meson_radius = 1.5;

    //Mass and width
    double rho_MASS     = 0.77526;
    double rho_WIDTH    = 0.1478;
    double rho_amp      = 1.0;
    double rho_phase    = 0.0;

    double omega_MASS   = 0.78265;
    double omega_WIDTH  = 0.00849;
    double omega_amp    = 1.0;
    double omega_phase  = 0.0;

    double f2_MASS     = 1.2755;
    double f2_WIDTH    = 0.1867;
    double f2_amp      = 1.0;
    double f2_phase    = 0.0;

    double sigma_MASS  = 0.480;
    double sigma_WIDTH = 0.350;
    double sigma_amp   = 1.0;
    double sigma_phase = 0.0;

    double f0_MASS     = 0.965;
    double f0_GPP      = 0.165;
    double f0_GKK      = 4.21*f0_GPP;
    double f0_amp      = 2.0;
    double f0_phase    = 0.0;

    //rho(770)
    Variable v_rho_Mass("rho_MASS",rho_MASS);
    Variable v_rho_Width("rho_WIDTH",rho_WIDTH);
    Variable v_rho_amp_real("rho_amp_real",rho_amp*cos(rho_phase));
    Variable v_rho_amp_img("rho_amp_img",rho_amp*sin(rho_phase));

    v_rho_Mass.setFixed(true);
    v_rho_Width.setFixed(true);

    //omega(782)
    Variable v_omega_Mass("omega_MASS",omega_MASS);
    Variable v_omega_Width("omega_WIDTH",omega_WIDTH);
    Variable v_omega_amp_real("omega_amp_real",omega_amp*cos(omega_phase),0.01,-100,+100);
    Variable v_omega_amp_img("omega_amp_img",omega_amp*sin(omega_phase),0.01,-100,+100);

    //f2(1270)
    Variable v_f2_Mass("f2_MASS",f2_MASS);
    Variable v_f2_Width("f2_WIDTH",f2_WIDTH);
    Variable v_f2_amp_real("f2_amp_real",f2_amp*cos(f2_phase),0.01,-100,+100);
    Variable v_f2_amp_img("f2_amp_img",f2_amp*sin(f2_phase),0.01,-100,+100);

    //sigma(480)
    Variable v_sigma_Mass("sigma_MASS",sigma_MASS);
    Variable v_sigma_Width("sigma_WIDTH",sigma_WIDTH);
    Variable v_sigma_amp_real("sigma_amp_real",sigma_amp*cos(sigma_phase),0.01,-100,+100);
    Variable v_sigma_amp_img("sigma_amp_img",sigma_amp*sin(sigma_phase),0.01,-100,+100);

    //f0(980)
    Variable v_f0_Mass("f0_MASS",f0_MASS);
    Variable v_f0_GPP("f0_GPP",f0_GPP);
    Variable v_f0_GKK("f0_GKK",f0_GKK);
    Variable v_f0_amp_real("f0_amp_real",f0_amp*cos(f0_phase),0.01,-100,+100);
    Variable v_f0_amp_img("f0_amp_img",f0_amp*sin(f0_phase),0.01,-100,+100);

    //NR

    Variable nonr_amp_real("nonr_amp_real", 1.0, 0.001, -100, +100);
    Variable nonr_amp_imag("nonr_amp_imag", 0.0, 0.001, -100, +100);

    //setting resonances
    ResonancePdf* rho_12 = new Resonances::GS("rho",v_rho_amp_real,v_rho_amp_img,v_rho_Mass,v_rho_Width,1,PAIR_12);
    ResonancePdf* rho_13 = new Resonances::GS("rho",v_rho_amp_real,v_rho_amp_img,v_rho_Mass,v_rho_Width,1,PAIR_13);

    ResonancePdf* omega_12 = new Resonances::RBW("omega",v_omega_amp_real,v_omega_amp_img,v_omega_Mass,v_omega_Width,1,PAIR_12);
    ResonancePdf* omega_13 = new Resonances::RBW("omega",v_omega_amp_real,v_omega_amp_img,v_omega_Mass,v_omega_Width,1,PAIR_13);

    ResonancePdf* f2_12 = new Resonances::RBW("f2",v_f2_amp_real,v_f2_amp_img,v_f2_Mass,v_f2_Width,2,PAIR_12);
    ResonancePdf* f2_13 = new Resonances::RBW("f2",v_f2_amp_real,v_f2_amp_img,v_f2_Mass,v_f2_Width,2,PAIR_13);

    ResonancePdf* sigma_12 = new Resonances::RBW("sigma",v_sigma_amp_real,v_sigma_amp_img,v_sigma_Mass,v_sigma_Width,(unsigned int)0,PAIR_12);
    ResonancePdf* sigma_13 = new Resonances::RBW("sigma",v_sigma_amp_real,v_sigma_amp_img,v_sigma_Mass,v_sigma_Width,(unsigned int)0,PAIR_13);

    ResonancePdf* f0_12 = new Resonances::FLATTE("f0",v_f0_amp_real,v_f0_amp_img,v_f0_Mass,v_f0_GPP,v_f0_GKK,PAIR_12,false);
    ResonancePdf* f0_13 = new Resonances::FLATTE("f0",v_f0_amp_real,v_f0_amp_img,v_f0_Mass,v_f0_GPP,v_f0_GKK,PAIR_13,false);

    ResonancePdf *nonr = new Resonances::NonRes("nonr", nonr_amp_real, nonr_amp_imag);

    //MIPWA
    ResonancePdf *swave_12 = loadPWAResonance(pwa_file, false);

    dtoppp.resonances.push_back(rho_12);
    //dtoppp.resonances.push_back(rho_13);
    //dtoppp.resonances.push_back(omega_12);
    //dtoppp.resonances.push_back(omega_13);
    //dtoppp.resonances.push_back(f2_12);
    //dtoppp.resonances.push_back(f2_13);
    //dtoppp.resonances.push_back(sigma_12);
    //dtoppp.resonances.push_back(sigma_13);
    //dtoppp.resonances.push_back(f0_12);
    //dtoppp.resonances.push_back(f0_13);
    //dtoppp.resonances.push_back(nonr); 
    dtoppp.resonances.push_back(swave_12);

    if(!eff) {
        // By default create a constant efficiency.
        vector<Variable> offsets;
        vector<Observable> observables;
        vector<Variable> coefficients;
        Variable constantOne("constantOne", 1);
        Variable constantZero("constantZero", 0);

        observables.push_back(s12);
        observables.push_back(s13);
        offsets.push_back(constantZero);
        offsets.push_back(constantZero);
        coefficients.push_back(constantOne);
        eff = new PolynomialPdf("constantEff", observables, coefficients, offsets, 0); //No efficiency
    }

    return new DalitzPlotPdf("signalPDF", s12, s13, eventNumber, dtoppp, eff);
}

void gettoydata(std::string name){

    std::cout << "get data begin!" << '\n';

    Data = new UnbinnedDataSet({s12,s13,eventNumber});

    std::ifstream reader(name.c_str());

    while(reader >> eventNumber >> s12 >> s13){
        Data->addEvent();
    }

    reader.close();
    std::cout << "get data end!" << '\n';
}

void runtoygen(std::string name, size_t events){

    s12.setNumBins(1500);
    s13.setNumBins(1500);

    signaldalitz = makesignalpdf(0);

    std::cout << "Creating Overall PDF" << '\n';
    ProdPdf* overallSignal = new ProdPdf("overallSignal",{signaldalitz});
    {
        maketoydalitzdata(overallSignal,name,events);
    }
}

void PrintFF(std::vector<std::vector<fptype>> ff){

    size_t nEntries = signaldalitz->getCachedWave(0).size();
    size_t n_res = signaldalitz->getDecayInfo().resonances.size();
    fptype sum = 0;

    std::cout << "nEntries= " << nEntries << '\n';
    for(size_t i = 0; i < n_res ; i++){

        for(size_t j = 0; j< n_res ; j++){
            std::cout << "FF[" << i << "," << j <<"]= " << ff[i][j] << std::endl;

        }

        sum+=ff[i][i];
    }

    std::cout << "Sum[i,i]= " << sum << std::endl;
}


void drawFitPlotsWithPulls(TH1 *hd, TH1 *ht, string plotdir) {
    const char *hname = hd->GetName();
    char obsname[10];
    for(int i = 0;; i++) {
        if(hname[i] == '_')
            obsname[i] = '\0';
        else
            obsname[i] = hname[i];
        if(obsname[i] == '\0')
            break;
    }
    ht->Scale(hd->Integral() / ht->Integral()*5);
    ht->SetLineColor(kRed);
    ht->SetLineWidth(3);
    ht->SetMarkerStyle(0);

    hd->SetMarkerColor(kBlack);
    hd->Rebin(5);


    TCanvas foo;

    hd->Draw("E");
    ht->Draw("HIST C same");


    foo.SaveAs(TString::Format("plots/%s_fit.png",obsname));


}


void makeToyDalitzPdfPlots(GooPdf *overallSignal, string plotdir = "plots") {
    TH1F s12_dat_hist("s12_dat_hist", "", s12.getNumBins(), s12.getLowerLimit(), s12.getUpperLimit());
    s12_dat_hist.GetXaxis()->SetTitle("m^{2}(K^{-} K^{+}) [GeV]");
    s12_dat_hist.GetYaxis()->SetTitle(TString::Format("Events / %.1f MeV", 1e3 * s12_dat_hist.GetBinWidth(1)));

    TH1F s12_pdf_hist("s12_pdf_hist", "", s12.getNumBins(), s12.getLowerLimit(), s12.getUpperLimit());

    TH1F s13_dat_hist("s13_dat_hist", "", s13.getNumBins(), s13.getLowerLimit(), s13.getUpperLimit());
    s13_dat_hist.GetXaxis()->SetTitle("m^{2}(K^{-} K^{+}) [GeV]");
    s13_dat_hist.GetYaxis()->SetTitle(TString::Format("Events / %.1f MeV", 1e3 * s13_dat_hist.GetBinWidth(1)));

    TH1F s13_pdf_hist("s13_pdf_hist", "", s13.getNumBins(), s13.getLowerLimit(), s13.getUpperLimit());

    TH1F s23_dat_hist("s23_dat_hist", "", s13.getNumBins(), s13.getLowerLimit(), s13.getUpperLimit());
    s23_dat_hist.GetXaxis()->SetTitle("m^{2}(#pi^{+} #pi^{-}) [GeV]");
    s23_dat_hist.GetYaxis()->SetTitle(TString::Format("Events / %.1f MeV", 1e3 * s13_dat_hist.GetBinWidth(1)));

    TH1F s23_pdf_hist("s23_pdf_hist", "", s13.getNumBins(), s13.getLowerLimit(), s13.getUpperLimit());

    double totalPdf = 0;
    double totalDat = 0;
    TH2F dalitzpp0_dat_hist("dalitzpp0_dat_hist",
                            "",
                            s12.getNumBins(),
                            s12.getLowerLimit(),
                            s12.getUpperLimit(),
                            s13.getNumBins(),
                            s13.getLowerLimit(),
                            s13.getUpperLimit());
    dalitzpp0_dat_hist.SetStats(false);
    dalitzpp0_dat_hist.GetXaxis()->SetTitle("m^{2}(K^{-} K^{+}) [GeV]");
    dalitzpp0_dat_hist.GetYaxis()->SetTitle("m^{2}(K^{-} K^{+}) [GeV^{2}]");
    TH2F dalitzpp0_pdf_hist("dalitzpp0_pdf_hist",
                            "",
                            s12.getNumBins(),
                            s12.getLowerLimit(),
                            s12.getUpperLimit(),
                            s13.getNumBins(),
                            s13.getLowerLimit(),
                            s13.getUpperLimit());

    dalitzpp0_pdf_hist.GetXaxis()->SetTitle("m^{2}(K^{-} K^{+}) [GeV^{2}]");
    dalitzpp0_pdf_hist.GetYaxis()->SetTitle("m^{2}(K^{-} K^{+}) [GeV^{2}]");
    dalitzpp0_pdf_hist.SetStats(false);
    std::vector<Observable> vars;
    vars.push_back(s12);
    vars.push_back(s13);
    vars.push_back(eventNumber);
    UnbinnedDataSet currData(vars);
    int evtCounter = 0;

    for(int i = 0; i < s12.getNumBins(); ++i) {
        s12.setValue(s12.getLowerLimit() + (s12.getUpperLimit() - s12.getLowerLimit()) * (i + 0.5) / s12.getNumBins());
        for(int j = 0; j < s13.getNumBins(); ++j) {
            s13.setValue(s13.getLowerLimit()
                         + (s13.getUpperLimit() - s13.getLowerLimit()) * (j + 0.5) / s13.getNumBins());
            if(!inDalitz(s12.getValue(), s13.getValue(), D_MASS, d1_MASS, d2_MASS, d3_MASS)){
                continue;}
            eventNumber.setValue(evtCounter);
            evtCounter++;
            currData.addEvent();
        }
    }
    overallSignal->setData(&currData);
    signaldalitz->setDataSize(currData.getNumEvents());
    std::vector<std::vector<double>> pdfValues = overallSignal->getCompProbsAtDataPoints();
    for(unsigned int j = 0; j < pdfValues[0].size(); ++j) {
        double currs12 = currData.getValue(s12, j);
        double currs13 = currData.getValue(s13, j);

        dalitzpp0_pdf_hist.Fill(currs12, currs13, pdfValues[0][j]);
        s12_pdf_hist.Fill(currs12, pdfValues[0][j]);
        s13_pdf_hist.Fill(currs13, pdfValues[0][j]);
        s23_pdf_hist.Fill(cpuGetM23(currs12, currs13), pdfValues[0][j]);
        totalPdf += pdfValues[0][j];
    }

    TCanvas foo;
    foo.SetLogz(false);
    dalitzpp0_pdf_hist.Draw("colz");

    foo.SaveAs("plots/dalitzpp0_pdf.png");

    for(unsigned int evt = 0; evt < Data->getNumEvents(); ++evt) {
        double data_s12 = Data->getValue(s12, evt);
        s12_dat_hist.Fill(data_s12);
        double data_s13 = Data->getValue(s13, evt);
        s13_dat_hist.Fill(data_s13);
        dalitzpp0_dat_hist.Fill(data_s12, data_s13);
        s23_dat_hist.Fill(cpuGetM23(data_s12, data_s13));
        totalDat++;
    }
    dalitzpp0_dat_hist.Draw("colz");
    foo.SaveAs("plots/dalitzpp0_dat.png");

    drawFitPlotsWithPulls(&s12_dat_hist, &s12_pdf_hist, plotdir);
    drawFitPlotsWithPulls(&s13_dat_hist, &s13_pdf_hist, plotdir);
    drawFitPlotsWithPulls(&s23_dat_hist, &s23_pdf_hist, plotdir);
}

void runMakeToyDalitzPdfPlots(std::string name){

    s12.setNumBins(1500);
    s13.setNumBins(1500);

    gettoydata(name);

    if(signaldalitz == nullptr){
        signaldalitz = makesignalpdf(0);
        }
    
    comps.clear();
    comps.push_back(signaldalitz);
    ProdPdf *overallsignal = new ProdPdf("overallsignal",comps);
    overallsignal->setData(Data);
    signaldalitz->setDataSize(Data->getNumEvents());

    makeToyDalitzPdfPlots(overallsignal);

}

void runtoyfit(std::string name){

    s12.setNumBins(1500);
    s13.setNumBins(1500);

    gettoydata(name);

    GOOFIT_INFO("Number of Events in dataset: {}", Data->getNumEvents());

    if(signaldalitz == nullptr){
    signaldalitz = makesignalpdf(0);
    }

    comps.clear();
    comps.push_back(signaldalitz);
    ProdPdf *overallsignal = new ProdPdf("overallsignal",comps);
    overallsignal->setData(Data);
    signaldalitz->setDataSize(Data->getNumEvents());

    FitManagerMinuit2 fitter(overallsignal);
    fitter.setVerbosity(3);
    
    auto func_min = fitter.fit();

    auto ff = signaldalitz->fit_fractions();
    PrintFF(ff);

    makeToyDalitzPdfPlots(overallsignal);

    /*signaldalitz->copyParams();
    signaldalitz->normalise();
    thrust::device_vector<fpcomplex> w1 = signaldalitz->getCachedWave(0);
    thrust::device_vector<fpcomplex> w2 = signaldalitz->getCachedWave(1);
    thrust::device_vector<fpcomplex> w3 = signaldalitz->getCachedWave(2);
    thrust::device_vector<fpcomplex> w4 = signaldalitz->getCachedWave(3);

    ofstream wr("pwa.txt");

    for(size_t i = 0 ; i < w1.size(); i++){

        fpcomplex v1 = w1[i];
        fpcomplex v2 = w2[i];
        fpcomplex v3 = w3[i];
        fpcomplex v4 = w4[i];
        
        fpcomplex v  = v1 + v2 + v3 + v4;

        wr << i  << '\t' << v.real() << '\t' << v.imag() << endl;
    }

    wr.close();
    */

}

int main(int argc, char **argv){

    GooFit::Application app{"D2PPP",argc,argv};

    size_t  nevents = 100000;
    auto gen = app.add_subcommand("gen","generate toy data");
    gen->add_option("-e,--events",nevents,"The number of events to generate",true);

    auto fit = app.add_subcommand("fit","fit toy data");

    auto plot = app.add_subcommand("plot","plot signal");

    GOOFIT_PARSE(app);

    /// Make the plot directory if it does not exist
    std::string command = "mkdir -p plots";
    if(system(command.c_str()) != 0)
        throw GooFit::GeneralError("Making `plots` directory failed");

    if(*gen){
        CLI::AutoTimer timer("MC Generation");
        runtoygen("D2PPP_toy.txt",nevents);
    }

    if(*fit){
        CLI::AutoTimer timer("FIT");
        runtoyfit("D2PPP_toy.txt");
    }

    if(*plot){
        CLI::AutoTimer timer("FIT");
        runMakeToyDalitzPdfPlots("D2PPP_toy.txt");
    }

}