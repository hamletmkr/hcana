/**
\class THcTrigDet
\ingroup Detectors

\brief A mock detector to hold trigger related data.

This class behaves as a detector, but it does not correspond to any physical
detector in the hall. Its purpose is to gather all the trigger related data
comming from a specific source, like HMS.

Can hold up to 100 ADC and TDC channels, though the limit can be changed if
needed. It just seemed like a reasonable starting value.

# Defined variables

For ADC channels it defines:
  - ADC value: `var_adc`
  - pedestal: `var_adcPed`
  - multiplicity: `var_adcMult`

For TDC channels it defines:
  - TDC value: `var_tdc`
  - multiplicity: `var_tdcMult`

# Parameter file variables

The names and number of channels is defined in a parameter file. The detector
looks for next variables:
  - `prefix_numAdc = number_of_ADC_channels`
  - `prefix_numTdc = number_of_TDC_channels`
  - `prefix_adcNames = "varName1 varName2 ... varNameNumAdc"`
  - `prefix_tdcNames = "varName1 varName2 ... varNameNumTdc"`

# Map file information

ADC channels must be assigned plane `1` and signal `0` while TDC channels must
be assigned plane `2` and signal `1`.

Each channel within a plane must be assigned a consecutive "bar" number, which
is then used to get the correct variable name from parameter file.

Use only with THcTrigApp class.
*/

/**
\fn THcTrigDet::THcTrigDet(
  const char* name, const char* description="",
  THaApparatus* app=NULL)

\brief A constructor.

\param[in] name Name of the apparatus. Is typically named after spectrometer
  whose trigger data is collecting; like "HMS".
\param[in] description Description of the apparatus.
\param[in] app The parent apparatus pointer.
*/

/**
\fn virtual THcTrigDet::~THcTrigDet()

\brief A destructor.
*/

/**
\fn virtual THaAnalysisObject::EStatus THcTrigDet::Init(const TDatime& date)

\brief Initializes the detector variables.

\param[in] date Time of the current run.
*/

/**
\fn virtual void THcTrigDet::Clear(Option_t* opt="")

\brief Clears variables before next event.

\param[in] opt Maybe used in base clas... Not sure.
*/

/**
\fn Int_t THcTrigDet::Decode(const THaEvData& evData)

\brief Decodes and processes events.

\param[in] evData Raw data to decode.
*/

//TODO: Check if fNumAdc < fMaxAdcChannels && fNumTdc < fMaxTdcChannels.

#include "THcTrigDet.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "TDatime.h"
#include "TString.h"

#include "THaApparatus.h"
#include "THaEvData.h"

#include "THcDetectorMap.h"
#include "THcGlobals.h"
#include "THcParmList.h"
#include "THcTrigApp.h"
#include "THcTrigRawHit.h"


THcTrigDet::THcTrigDet() {}


THcTrigDet::THcTrigDet(
  const char* name, const char* description, THaApparatus* app
) :
  THaDetector(name, description, app), THcHitList(),
  fKwPrefix(""),
  fNumAdc(0), fNumTdc(0), fAdcNames(), fTdcNames(),
  fAdcVal(), fAdcPedestal(), fAdcMultiplicity(),
  fTdcVal(), fTdcMultiplicity()
{}


THcTrigDet::~THcTrigDet() {}


THaAnalysisObject::EStatus THcTrigDet::Init(const TDatime& date) {
  // Call `Setup` before everything else.
  Setup(GetName(), GetTitle());

  // Initialize all variables.
  for (int i=0; i<fMaxAdcChannels; ++i) fAdcVal[i] = -1.0;
  for (int i=0; i<fMaxTdcChannels; ++i) fTdcVal[i] = -1.0;

  // Call initializer for base class.
  // This also calls `ReadDatabase` and `DefineVariables`.
  EStatus status = THaDetector::Init(date);
  if (status) {
    fStatus = status;
    return fStatus;
  }

  // Initialize hitlist part of the class.
  InitHitList(fDetMap, "THcTrigRawHit", 100);

  // Fill in detector map.
  string EngineDID = string(GetApparatus()->GetName()).substr(0, 1) + GetName();
  std::transform(EngineDID.begin(), EngineDID.end(), EngineDID.begin(), ::toupper);
  if (gHcDetectorMap->FillMap(fDetMap, EngineDID.c_str()) < 0) {
    static const char* const here = "Init()";
    Error(Here(here), "Error filling detectormap for %s.", EngineDID.c_str());
    return kInitError;
  }

  fStatus = kOK;
  return fStatus;
}


void THcTrigDet::Clear(Option_t* opt) {
  THaAnalysisObject::Clear(opt);

  // Reset all data.
  for (int i=0; i<fNumAdc; ++i) fAdcVal[i] = 0.0;
  for (int i=0; i<fNumTdc; ++i) fTdcVal[i] = 0.0;
}


Int_t THcTrigDet::Decode(const THaEvData& evData) {
  // Decode raw data for this event.
  Int_t numHits = DecodeToHitList(evData);

  // Process each hit and fill variables.
  Int_t iHit = 0;
  while (iHit < numHits) {
    THcTrigRawHit* hit = dynamic_cast<THcTrigRawHit*>(fRawHitList->At(iHit));

    if (hit->fPlane == 1) {
      fAdcVal[hit->fCounter-1] = hit->GetData(0, 0);
      fAdcPedestal[hit->fCounter-1] = hit->GetAdcPedestal(0);
      fAdcMultiplicity[hit->fCounter-1] = hit->GetMultiplicity(0);
    }
    else if (hit->fPlane == 2) {
      fTdcVal[hit->fCounter-1] = hit->GetData(1, 0);
      fTdcMultiplicity[hit->fCounter-1] = hit->GetMultiplicity(1);
    }
    else {
      throw std::out_of_range(
        "`THcTrigDet::Decode`: only planes `1` and `2` available!"
      );
    }

    ++iHit;
  }

  return 0;
}


void THcTrigDet::Setup(const char* name, const char* description) {
  // Prefix for parameters in `param` file.
  string kwPrefix = string(GetApparatus()->GetName()) + "_" + name;
  std::transform(kwPrefix.begin(), kwPrefix.end(), kwPrefix.begin(), ::tolower);
  fKwPrefix = kwPrefix;
}


Int_t THcTrigDet::ReadDatabase(const TDatime& date) {
  std::string adcNames, tdcNames;

  DBRequest list[] = {
    {"_numAdc", &fNumAdc, kInt},  // Number of ADC channels.
    {"_numTdc", &fNumTdc, kInt},  // Number of TDC channels.
    {"_adcNames", &adcNames, kString},  // Names of ADC channels.
    {"_tdcNames", &tdcNames, kString},  // Names of TDC channels.
    {0}
  };
  gHcParms->LoadParmValues(list, fKwPrefix.c_str());

  // Split the names to std::vector<std::string>.
  fAdcNames = vsplit(adcNames);
  fTdcNames = vsplit(tdcNames);

  return kOK;
}


Int_t THcTrigDet::DefineVariables(THaAnalysisObject::EMode mode) {
  if (mode == kDefine && fIsSetup) return kOK;
  fIsSetup = (mode == kDefine);

  std::vector<RVarDef> vars;

  //Push the variable names for ADC channels.
  std::vector<TString> adcValTitle(fNumAdc), adcValVar(fNumAdc);
  std::vector<TString> adcPedestalTitle(fNumAdc), adcPedestalVar(fNumAdc);
  std::vector<TString> adcMultiplicityTitle(fNumAdc), adcMultiplicityVar(fNumAdc);

  for (int i=0; i<fNumAdc; ++i) {
    adcValTitle.at(i) = fAdcNames.at(i) + "_adc";
    adcValVar.at(i) = TString::Format("fAdcVal[%d]", i);
    RVarDef entry1 {
      adcValTitle.at(i).Data(),
      adcValTitle.at(i).Data(),
      adcValVar.at(i).Data()
    };
    vars.push_back(entry1);

    adcPedestalTitle.at(i) = fAdcNames.at(i) + "_adcPed";
    adcPedestalVar.at(i) = TString::Format("fAdcPedestal[%d]", i);
    RVarDef entry2 {
      adcPedestalTitle.at(i).Data(),
      adcPedestalTitle.at(i).Data(),
      adcPedestalVar.at(i).Data()
    };
    vars.push_back(entry2);

    adcMultiplicityTitle.at(i) = fAdcNames.at(i) + "_adcMult";
    adcMultiplicityVar.at(i) = TString::Format("fAdcMultiplicity[%d]", i);
    RVarDef entry3 {
      adcMultiplicityTitle.at(i).Data(),
      adcMultiplicityTitle.at(i).Data(),
      adcMultiplicityVar.at(i).Data()
    };
    vars.push_back(entry3);
  }

  // Push the variable names for TDC channels.
  std::vector<TString> tdcValTitle(fNumTdc), tdcValVar(fNumTdc);
  std::vector<TString> tdcMultiplicityTitle(fNumTdc), tdcMultiplicityVar(fNumTdc);
  for (int i=0; i<fNumTdc; ++i) {
    tdcValTitle.at(i) = fTdcNames.at(i) + "_tdc";
    tdcValVar.at(i) = TString::Format("fTdcVal[%d]", i);
    RVarDef entry1 {
      tdcValTitle.at(i).Data(),
      tdcValTitle.at(i).Data(),
      tdcValVar.at(i).Data()
    };
    vars.push_back(entry1);

    tdcMultiplicityTitle.at(i) = fTdcNames.at(i) + "_tdcMult";
    tdcMultiplicityVar.at(i) = TString::Format("fTdcMultiplicity[%d]", i);
    RVarDef entry2 {
      tdcMultiplicityTitle.at(i).Data(),
      tdcMultiplicityTitle.at(i).Data(),
      tdcMultiplicityVar.at(i).Data()
    };
    vars.push_back(entry2);
  }

  RVarDef end {0};
  vars.push_back(end);

  return DefineVarsFromList(vars.data(), mode);
}


ClassImp(THcTrigDet)
