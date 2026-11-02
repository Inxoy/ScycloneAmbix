//
// Created by valentin.ackva on 10.11.2022.
//
#include "PluginParameters.h"

juce::AudioProcessorValueTreeState::ParameterLayout PluginParameters::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;


    params.push_back (std::make_unique<juce::AudioParameterFloat> (INPUT_GAIN_ID,
                                                                   INPUT_GAIN_NAME,
                                                                   gainRange,
                                                                   0.f,
                                                                   gain_attributes));

    params.push_back(std::make_unique<juce::AudioParameterFloat> (TRAN_ATTACK_TIME_NETWORK1_ID,
                                                                  TRAN_ATTACK_TIME_NETWORK1_NAME,
                                                                  tranAttackTimeRange,
                                                                  .5f,
                                                                  ms_attributes));

    params.push_back(std::make_unique<juce::AudioParameterFloat> (TRAN_ATTACK_TIME_NETWORK2_ID,
                                                                  TRAN_ATTACK_TIME_NETWORK2_NAME,
                                                                  tranAttackTimeRange,
                                                                  .5f,
                                                                  ms_attributes));

    params.push_back(std::make_unique<juce::AudioParameterFloat> (TRAN_SHAPER_NETWORK1_ID,
                                                                  TRAN_SHAPER_NETWORK1_NAME,
                                                                  tranShaperRange,
                                                                  0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat> (FILTER_NETWORK1_ID,
                                                                  FILTER_NETWORK1_NAME,
                                                                  filterRange,
                                                                  0.7f,
                                                                  percentage_attributes));

    params.push_back(std::make_unique<juce::AudioParameterFloat> (TRAN_SHAPER_NETWORK2_ID,
                                                                  TRAN_SHAPER_NETWORK2_NAME,
                                                                  tranShaperRange,
                                                                  0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat> (FILTER_NETWORK2_ID,
                                                                  FILTER_NETWORK2_NAME,
                                                                  filterRange,
                                                                  0.3f,
                                                                  percentage_attributes));

    params.push_back(std::make_unique<juce::AudioParameterBool>(SELECT_NETWORK1_ID,
                                                                SELECT_NETWORK1_NAME,
                                                                false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(GRAIN_ON_OFF_NETWORK1_ID,
                                                                GRAIN_ON_OFF_NETWORK1_NAME,
                                                                false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(ON_OFF_NETWORK1_ID,
                                                                ON_OFF_NETWORK1_NAME,
                                                                true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(SELECT_NETWORK2_ID,
                                                                SELECT_NETWORK2_NAME,
                                                                false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(GRAIN_ON_OFF_NETWORK2_ID,
                                                                GRAIN_ON_OFF_NETWORK2_NAME,
                                                                false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(ON_OFF_NETWORK2_ID,
                                                                ON_OFF_NETWORK2_NAME,
                                                                false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(GRAIN_NETWORK1_INTERVAL_ID,
                                                                 GRAIN_NETWORK1_INTERVAL_NAME,
                                                                 grainIntervalRange,
                                                                 0.5f,
                                                                 ms_format));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(GRAIN_NETWORK1_SIZE_ID,
                                                                 GRAIN_NETWORK1_SIZE_NAME,
                                                                 grainSizeRange,
                                                                 60.f,
                                                                 ms_format));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(GRAIN_NETWORK1_PITCH_ID,
                                                                 GRAIN_NETWORK1_PITCH_NAME,
                                                                 grainPitchRange,
                                                                 0.0f,
                                                                 semitones_attributes));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(GRAIN_NETWORK1_MIX_ID,
                                                                 GRAIN_NETWORK1_MIX_NAME,
                                                                 dryWetRange,
                                                                 0.25f,
                                                                 percentage_attributes));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(GRAIN_NETWORK2_INTERVAL_ID,
                                                                 GRAIN_NETWORK2_INTERVAL_NAME,
                                                                 grainIntervalRange,
                                                                 0.5f,
                                                                 ms_format));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(GRAIN_NETWORK2_SIZE_ID,
                                                                 GRAIN_NETWORK2_SIZE_NAME,
                                                                 grainSizeRange,
                                                                 60.0f,
                                                                 ms_format));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(GRAIN_NETWORK2_PITCH_ID,
                                                                 GRAIN_NETWORK2_PITCH_NAME,
                                                                 grainPitchRange,
                                                                 0.0f,
                                                                 semitones_attributes));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(GRAIN_NETWORK2_MIX_ID,
                                                                 GRAIN_NETWORK2_MIX_NAME,
                                                                 dryWetRange,
                                                                 0.25f,
                                                                 percentage_attributes));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(FADE_ID,
                                                                 FADE_NAME,
                                                                 fadeRange,
                                                                 1.f,
                                                                 fade_attributes));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(COMP_DRY_WET_ID,
                                                                 COMP_DRY_WET_NAME,
                                                                 dryWetRange,
                                                                 1.0f,
                                                                 percentage_attributes));

    params.push_back( std::make_unique<juce::AudioParameterFloat>  (COMP_THRESHOLD_ID,
                                                                    COMP_THRESHOLD_NAME,
                                                                    compThresholdRange,
                                                                    -20.0f,
                                                                    dB_attributes));

    params.push_back( std::make_unique<juce::AudioParameterFloat>  (COMP_RATIO_ID,
                                                                    COMP_RATIO_NAME,
                                                                    compRatioRange,
                                                                    4.0f,
                                                                    ratio_attributes));

    params.push_back( std::make_unique<juce::AudioParameterFloat>  (COMP_MAKEUPGAIN_ID,
                                                                    COMP_MAKEUPGAIN_NAME,
                                                                    compMakeupRange,
                                                                    -0.1f,
                                                                    makeup_attributes));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (OUTPUT_GAIN_ID,
                                                                   OUTPUT_GAIN_NAME,
                                                                   gainRange,
                                                                   0.f,
                                                                   gain_attributes));


    params.push_back( std::make_unique<juce::AudioParameterFloat>  (DRY_WET_ID,
                                                              DRY_WET_NAME,
                                                              dryWetRange,
                                                              0.7f,
                                                              percentage_attributes));

    // ambisonic (AmbiX) encoding, each network is a point source steered by its latent space
    struct AmbiParameterSet {
        juce::ParameterID azimuthID, elevationID, spreadID, azimuthDepthID, elevationDepthID, spreadDepthID,
                azimuthLatentID, elevationLatentID, spreadLatentID;
        juce::String azimuthName, elevationName, spreadName, azimuthDepthName, elevationDepthName, spreadDepthName,
                azimuthLatentName, elevationLatentName, spreadLatentName;
    };

    const AmbiParameterSet ambiParameterSets[] = {
            {AMBI_AZIMUTH_NETWORK1_ID, AMBI_ELEVATION_NETWORK1_ID, AMBI_SPREAD_NETWORK1_ID,
             AMBI_AZIMUTH_DEPTH_NETWORK1_ID, AMBI_ELEVATION_DEPTH_NETWORK1_ID, AMBI_SPREAD_DEPTH_NETWORK1_ID,
             AMBI_AZIMUTH_LATENT_NETWORK1_ID, AMBI_ELEVATION_LATENT_NETWORK1_ID, AMBI_SPREAD_LATENT_NETWORK1_ID,
             AMBI_AZIMUTH_NETWORK1_NAME, AMBI_ELEVATION_NETWORK1_NAME, AMBI_SPREAD_NETWORK1_NAME,
             AMBI_AZIMUTH_DEPTH_NETWORK1_NAME, AMBI_ELEVATION_DEPTH_NETWORK1_NAME, AMBI_SPREAD_DEPTH_NETWORK1_NAME,
             AMBI_AZIMUTH_LATENT_NETWORK1_NAME, AMBI_ELEVATION_LATENT_NETWORK1_NAME, AMBI_SPREAD_LATENT_NETWORK1_NAME},
            {AMBI_AZIMUTH_NETWORK2_ID, AMBI_ELEVATION_NETWORK2_ID, AMBI_SPREAD_NETWORK2_ID,
             AMBI_AZIMUTH_DEPTH_NETWORK2_ID, AMBI_ELEVATION_DEPTH_NETWORK2_ID, AMBI_SPREAD_DEPTH_NETWORK2_ID,
             AMBI_AZIMUTH_LATENT_NETWORK2_ID, AMBI_ELEVATION_LATENT_NETWORK2_ID, AMBI_SPREAD_LATENT_NETWORK2_ID,
             AMBI_AZIMUTH_NETWORK2_NAME, AMBI_ELEVATION_NETWORK2_NAME, AMBI_SPREAD_NETWORK2_NAME,
             AMBI_AZIMUTH_DEPTH_NETWORK2_NAME, AMBI_ELEVATION_DEPTH_NETWORK2_NAME, AMBI_SPREAD_DEPTH_NETWORK2_NAME,
             AMBI_AZIMUTH_LATENT_NETWORK2_NAME, AMBI_ELEVATION_LATENT_NETWORK2_NAME, AMBI_SPREAD_LATENT_NETWORK2_NAME}
    };

    for (const auto & set : ambiParameterSets) {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(set.azimuthID, set.azimuthName,
                                                                     azimuthRange, 0.f, degree_attributes));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(set.elevationID, set.elevationName,
                                                                     elevationRange, 0.f, degree_attributes));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(set.spreadID, set.spreadName,
                                                                     dryWetRange, 0.f, percentage_attributes));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(set.azimuthDepthID, set.azimuthDepthName,
                                                                     dryWetRange, 0.5f, percentage_attributes));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(set.elevationDepthID, set.elevationDepthName,
                                                                     dryWetRange, 0.25f, percentage_attributes));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(set.spreadDepthID, set.spreadDepthName,
                                                                     dryWetRange, 0.f, percentage_attributes));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(set.azimuthLatentID, set.azimuthLatentName,
                                                                      getLatentDimensionChoices(), 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(set.elevationLatentID, set.elevationLatentName,
                                                                      getLatentDimensionChoices(), 1));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(set.spreadLatentID, set.spreadLatentName,
                                                                      getLatentDimensionChoices(), 2));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(AMBI_SMOOTHING_ID,
                                                                 AMBI_SMOOTHING_NAME,
                                                                 ambiSmoothingRange,
                                                                 150.f,
                                                                 ms_smoothing_attributes));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(AMBI_DRY_WIDTH_ID,
                                                                 AMBI_DRY_WIDTH_NAME,
                                                                 dryWidthRange,
                                                                 30.f,
                                                                 degree_attributes));

    for (const auto & param : params) {
        parameterList.add(param->getParameterID());
    }

    return { params.begin(), params.end() };
}

juce::ValueTree PluginParameters::createNotAutomatableParameterLayout()
{
    juce::ValueTree notAutomatableParameters = juce::ValueTree("Settings");
    notAutomatableParameters.setProperty(ADVANCED_PARAMETER_CONTROL_VISIBLE_NAME, juce::var(false), nullptr);
    notAutomatableParameters.setProperty(NETWORK1_NAME_NAME, juce::var("Funk"), nullptr);
    notAutomatableParameters.setProperty(NETWORK2_NAME_NAME, juce::var("Djembe"), nullptr);
    return notAutomatableParameters;
}

void PluginParameters::removeNotAutomatableParameterLayout(juce::ValueTree notAutomatableParameters) {
    notAutomatableParameters.removeProperty(ADVANCED_PARAMETER_CONTROL_VISIBLE_NAME, nullptr);
    notAutomatableParameters.removeProperty(NETWORK1_NAME_NAME, nullptr);
    notAutomatableParameters.removeProperty(NETWORK2_NAME_NAME, nullptr);
}

juce::StringArray PluginParameters::getLatentDimensionChoices() {
    juce::StringArray choices;
    for (int i = 1; i <= 16; ++i)
        choices.add("Latent " + juce::String(i));
    return choices;
}

juce::StringArray PluginParameters::getPluginParameterList() {
    return parameterList;
}
