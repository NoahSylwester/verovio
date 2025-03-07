/////////////////////////////////////////////////////////////////////////////
// Name:        doc.cpp
// Author:      Laurent Pugin
// Created:     2005
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "doc.h"

//----------------------------------------------------------------------------

#include <cassert>
#include <math.h>

//----------------------------------------------------------------------------

#include "barline.h"
#include "beatrpt.h"
#include "chord.h"
#include "comparison.h"
#include "docselection.h"
#include "expansion.h"
#include "featureextractor.h"
#include "functorparams.h"
#include "glyph.h"
#include "instrdef.h"
#include "iomei.h"
#include "keysig.h"
#include "label.h"
#include "layer.h"
#include "mdiv.h"
#include "measure.h"
#include "mensur.h"
#include "metersig.h"
#include "mnum.h"
#include "mrest.h"
#include "mrpt.h"
#include "mrpt2.h"
#include "multirest.h"
#include "multirpt.h"
#include "note.h"
#include "page.h"
#include "pages.h"
#include "pgfoot.h"
#include "pgfoot2.h"
#include "pghead.h"
#include "pghead2.h"
#include "runningelement.h"
#include "score.h"
#include "slur.h"
#include "smufl.h"
#include "staff.h"
#include "staffdef.h"
#include "staffgrp.h"
#include "syl.h"
#include "syllable.h"
#include "system.h"
#include "text.h"
#include "timemap.h"
#include "timestamp.h"
#include "transposition.h"
#include "verse.h"
#include "vrv.h"
#include "zone.h"

//----------------------------------------------------------------------------

#include "MidiEvent.h"
#include "MidiFile.h"

namespace vrv {

//----------------------------------------------------------------------------
// Doc
//----------------------------------------------------------------------------

Doc::Doc() : Object(DOC, "doc-")
{
    m_options = new Options();

    // owned pointers need to be set to NULL;
    m_selectionPreceeding = NULL;
    m_selectionFollowing = NULL;

    this->Reset();
}

Doc::~Doc()
{
    this->ClearSelectionPages();

    delete m_options;
}

void Doc::Reset()
{
    Object::Reset();

    this->ClearSelectionPages();

    m_type = Raw;
    m_notationType = NOTATIONTYPE_NONE;
    m_pageHeight = -1;
    m_pageWidth = -1;
    m_pageMarginBottom = 0;
    m_pageMarginRight = 0;
    m_pageMarginLeft = 0;
    m_pageMarginTop = 0;

    m_drawingPageHeight = -1;
    m_drawingPageWidth = -1;
    m_drawingPageContentHeight = -1;
    m_drawingPageContentWidth = -1;
    m_drawingPageMarginBottom = 0;
    m_drawingPageMarginRight = 0;
    m_drawingPageMarginLeft = 0;
    m_drawingPageMarginTop = 0;

    m_drawingPage = NULL;
    m_currentScore = NULL;
    m_currentScoreDefDone = false;
    m_dataPreparationDone = false;
    m_timemapTempo = 0.0;
    m_markup = MARKUP_DEFAULT;
    m_isMensuralMusicOnly = false;
    m_isCastOff = false;

    m_facsimile = NULL;

    m_drawingSmuflFontSize = 0;
    m_drawingLyricFontSize = 0;

    m_header.reset();
    m_front.reset();
    m_back.reset();
}

void Doc::ClearSelectionPages()
{
    if (m_selectionPreceeding) {
        delete m_selectionPreceeding;
        m_selectionPreceeding = NULL;
    }
    if (m_selectionFollowing) {
        delete m_selectionFollowing;
        m_selectionFollowing = NULL;
    }
    m_selectionStart = "";
    m_selectionEnd = "";
}

void Doc::SetType(DocType type)
{
    m_type = type;
}

bool Doc::IsSupportedChild(Object *child)
{
    if (child->Is(MDIV)) {
        assert(dynamic_cast<Mdiv *>(child));
    }
    else if (child->Is(PAGES)) {
        assert(dynamic_cast<Pages *>(child));
    }
    else {
        return false;
    }
    return true;
}

void Doc::Refresh()
{
    RefreshViews();
}

bool Doc::GenerateDocumentScoreDef()
{
    Measure *measure = dynamic_cast<Measure *>(this->FindDescendantByType(MEASURE));
    if (!measure) {
        LogError("No measure found for generating a scoreDef");
        return false;
    }

    ListOfObjects staves = measure->FindAllDescendantsByType(STAFF, false);

    if (staves.empty()) {
        LogError("No staff found for generating a scoreDef");
        return false;
    }

    this->GetCurrentScoreDef()->Reset();
    StaffGrp *staffGrp = new StaffGrp();
    for (auto &object : staves) {
        Staff *staff = vrv_cast<Staff *>(object);
        assert(staff);
        StaffDef *staffDef = new StaffDef();
        staffDef->SetN(staff->GetN());
        staffDef->SetLines(5);
        if (!measure->IsMeasuredMusic()) staffDef->SetNotationtype(NOTATIONTYPE_mensural);
        staffGrp->AddChild(staffDef);
    }
    this->GetCurrentScoreDef()->AddChild(staffGrp);

    LogMessage("ScoreDef generated");

    return true;
}

bool Doc::GenerateFooter()
{
    if (this->GetCurrentScoreDef()->FindDescendantByType(PGFOOT)) {
        return false;
    }

    PgFoot *pgFoot = new PgFoot();
    // We mark it as generated for not having it written in the output
    pgFoot->IsGenerated(true);
    pgFoot->LoadFooter(this);
    pgFoot->SetType("autogenerated");
    this->GetCurrentScoreDef()->AddChild(pgFoot);

    PgFoot2 *pgFoot2 = new PgFoot2();
    pgFoot2->IsGenerated(true);
    pgFoot2->LoadFooter(this);
    pgFoot2->SetType("autogenerated");
    this->GetCurrentScoreDef()->AddChild(pgFoot2);

    return true;
}

bool Doc::GenerateHeader()
{
    if (this->GetCurrentScoreDef()->FindDescendantByType(PGHEAD)) return false;

    PgHead *pgHead = new PgHead();
    // We mark it as generated for not having it written in the output
    pgHead->IsGenerated(true);
    pgHead->GenerateFromMEIHeader(m_header);
    pgHead->SetType("autogenerated");
    this->GetCurrentScoreDef()->AddChild(pgHead);

    PgHead2 *pgHead2 = new PgHead2();
    pgHead2->IsGenerated(true);
    pgHead2->AddPageNum(HORIZONTALALIGNMENT_center, VERTICALALIGNMENT_top);
    pgHead2->SetType("autogenerated");
    this->GetCurrentScoreDef()->AddChild(pgHead2);

    return true;
}

bool Doc::GenerateMeasureNumbers()
{
    ListOfObjects measures = this->FindAllDescendantsByType(MEASURE, false);

    // run through all measures and generate missing mNum from attribute
    for (auto &object : measures) {
        Measure *measure = dynamic_cast<Measure *>(object);
        if (measure->HasN() && !measure->FindDescendantByType(MNUM)) {
            MNum *mnum = new MNum;
            Text *text = new Text;
            text->SetText(UTF8to16(measure->GetN()));
            mnum->SetType("autogenerated");
            mnum->AddChild(text);
            mnum->IsGenerated(true);
            measure->AddChild(mnum);
        }
    }

    return true;
}

bool Doc::HasTimemap() const
{
    return (m_timemapTempo == m_options->m_midiTempoAdjustment.GetValue());
}

void Doc::CalculateTimemap()
{
    m_timemapTempo = 0.0;

    // This happens if the document was never cast off (breaks none option in the toolkit)
    if (!m_drawingPage && this->GetPageCount() == 1) {
        Page *page = this->SetDrawingPage(0);
        if (!page) {
            return;
        }
        this->ScoreDefSetCurrentDoc();
        page->LayOutHorizontally();
    }

    double tempo = MIDI_TEMPO;

    // Set tempo
    if (this->GetCurrentScoreDef()->HasMidiBpm()) {
        tempo = this->GetCurrentScoreDef()->GetMidiBpm();
    }

    // We first calculate the maximum duration of each measure
    InitMaxMeasureDurationParams initMaxMeasureDurationParams;
    initMaxMeasureDurationParams.m_currentTempo = tempo;
    initMaxMeasureDurationParams.m_tempoAdjustment = m_options->m_midiTempoAdjustment.GetValue();
    Functor initMaxMeasureDuration(&Object::InitMaxMeasureDuration);
    Functor initMaxMeasureDurationEnd(&Object::InitMaxMeasureDurationEnd);
    this->Process(&initMaxMeasureDuration, &initMaxMeasureDurationParams, &initMaxMeasureDurationEnd);

    // Then calculate the onset and offset times (w.r.t. the measure) for every note
    InitOnsetOffsetParams initOnsetOffsetParams;
    Functor initOnsetOffset(&Object::InitOnsetOffset);
    Functor initOnsetOffsetEnd(&Object::InitOnsetOffsetEnd);
    this->Process(&initOnsetOffset, &initOnsetOffsetParams, &initOnsetOffsetEnd);

    // Adjust the duration of tied notes
    Functor initTimemapTies(&Object::InitTimemapTies);
    this->Process(&initTimemapTies, NULL, NULL, NULL, UNLIMITED_DEPTH, BACKWARD);

    m_timemapTempo = m_options->m_midiTempoAdjustment.GetValue();
}

void Doc::ExportMIDI(smf::MidiFile *midiFile)
{

    if (!Doc::HasTimemap()) {
        // generate MIDI timemap before progressing
        CalculateTimemap();
    }
    if (!Doc::HasTimemap()) {
        LogWarning("Calculation of MIDI timemap failed, not exporting MidiFile.");
    }

    double tempo = MIDI_TEMPO;

    // set MIDI tempo
    if (this->GetCurrentScoreDef()->HasMidiBpm()) {
        tempo = this->GetCurrentScoreDef()->GetMidiBpm();
    }
    midiFile->addTempo(0, 0, tempo);

    // Capture information for MIDI generation, i.e. from control elements
    Functor initMIDI(&Object::InitMIDI);
    InitMIDIParams initMIDIParams;
    initMIDIParams.m_currentTempo = tempo;
    this->Process(&initMIDI, &initMIDIParams);

    // We need to populate processing lists for processing the document by Layer (by Verse will not be used)
    InitProcessingListsParams initProcessingListsParams;
    // Alternate solution with StaffN_LayerN_VerseN_t (see also Verse::PrepareData)
    // StaffN_LayerN_VerseN_t staffLayerVerseTree;
    // params.push_back(&staffLayerVerseTree);

    // We first fill a tree of int with [staff/layer] and [staff/layer/verse] numbers (@n) to be process
    Functor initProcessingLists(&Object::InitProcessingLists);
    this->Process(&initProcessingLists, &initProcessingListsParams);

    // The tree is used to process each staff/layer/verse separately
    // For this, we use a array of AttNIntegerComparison that looks for each object if it is of the type
    // and with @n specified

    IntTree_t::iterator staves;
    IntTree_t::iterator layers;

    // Process notes and chords, rests, spaces layer by layer
    // track 0 (included by default) is reserved for meta messages common to all tracks
    int midiChannel = 0;
    int midiTrack = 1;
    Filters filters;
    for (staves = initProcessingListsParams.m_layerTree.child.begin();
         staves != initProcessingListsParams.m_layerTree.child.end(); ++staves) {

        ScoreDef *currentScoreDef = this->GetCurrentScoreDef();
        int transSemi = 0;
        if (StaffDef *staffDef = currentScoreDef->GetStaffDef(staves->first)) {
            // get the transposition (semi-tone) value for the staff
            if (staffDef->HasTransSemi()) transSemi = staffDef->GetTransSemi();
            midiTrack = staffDef->GetN();
            if (midiFile->getTrackCount() < (midiTrack + 1)) {
                midiFile->addTracks(midiTrack + 1 - midiFile->getTrackCount());
            }
            // set MIDI channel and instrument
            InstrDef *instrdef = dynamic_cast<InstrDef *>(staffDef->FindDescendantByType(INSTRDEF, 1));
            if (!instrdef) {
                StaffGrp *staffGrp = vrv_cast<StaffGrp *>(staffDef->GetFirstAncestor(STAFFGRP));
                assert(staffGrp);
                instrdef = dynamic_cast<InstrDef *>(staffGrp->FindDescendantByType(INSTRDEF, 1));
            }
            if (instrdef) {
                if (instrdef->HasMidiChannel()) midiChannel = instrdef->GetMidiChannel();
                if (instrdef->HasMidiTrack()) {
                    midiTrack = instrdef->GetMidiTrack();
                    if (midiFile->getTrackCount() < (midiTrack + 1)) {
                        midiFile->addTracks(midiTrack + 1 - midiFile->getTrackCount());
                    }
                    if (midiTrack > 255) {
                        LogWarning("A high MIDI track number was assigned to staff %d", staffDef->GetN());
                    }
                }
                if (instrdef->HasMidiInstrnum()) {
                    midiFile->addPatchChange(midiTrack, 0, midiChannel, instrdef->GetMidiInstrnum());
                }
            }
            // set MIDI track name
            Label *label = vrv_cast<Label *>(staffDef->FindDescendantByType(LABEL, 1));
            if (!label) {
                StaffGrp *staffGrp = vrv_cast<StaffGrp *>(staffDef->GetFirstAncestor(STAFFGRP));
                assert(staffGrp);
                label = vrv_cast<Label *>(staffGrp->FindDescendantByType(LABEL, 1));
            }
            if (label) {
                std::string trackName = UTF16to8(label->GetText(label)).c_str();
                if (!trackName.empty()) midiFile->addTrackName(midiTrack, 0, trackName);
            }
            // set MIDI key signature
            KeySig *keySig = vrv_cast<KeySig *>(staffDef->FindDescendantByType(KEYSIG));
            if (!keySig && (currentScoreDef->HasKeySigInfo())) {
                keySig = vrv_cast<KeySig *>(currentScoreDef->GetKeySig());
            }
            if (keySig && keySig->HasSig()) {
                midiFile->addKeySignature(midiTrack, 0, keySig->GetFifthsInt(), (keySig->GetMode() == MODE_minor));
            }
            // set MIDI time signature
            MeterSig *meterSig = vrv_cast<MeterSig *>(staffDef->FindDescendantByType(METERSIG));
            if (!meterSig && (currentScoreDef->HasMeterSigInfo())) {
                meterSig = vrv_cast<MeterSig *>(currentScoreDef->GetMeterSig());
            }
            if (meterSig && meterSig->HasCount()) {
                midiFile->addTimeSignature(midiTrack, 0, meterSig->GetTotalCount(), meterSig->GetUnit());
            }
        }

        // Set initial scoreDef values for tuning
        Functor generateScoreDefMIDI(&Object::GenerateMIDI);
        Functor generateScoreDefMIDIEnd(&Object::GenerateMIDIEnd);
        GenerateMIDIParams generateScoreDefMIDIParams(midiFile, &generateScoreDefMIDI);
        generateScoreDefMIDIParams.m_midiChannel = midiChannel;
        generateScoreDefMIDIParams.m_midiTrack = midiTrack;
        currentScoreDef->Process(&generateScoreDefMIDI, &generateScoreDefMIDIParams, &generateScoreDefMIDIEnd);

        for (layers = staves->second.child.begin(); layers != staves->second.child.end(); ++layers) {
            filters.Clear();
            // Create ad comparison object for each type / @n
            AttNIntegerComparison matchStaff(STAFF, staves->first);
            AttNIntegerComparison matchLayer(LAYER, layers->first);
            filters.Add(&matchStaff);
            filters.Add(&matchLayer);

            Functor generateMIDI(&Object::GenerateMIDI);
            Functor generateMIDIEnd(&Object::GenerateMIDIEnd);
            GenerateMIDIParams generateMIDIParams(midiFile, &generateMIDI);
            generateMIDIParams.m_midiChannel = midiChannel;
            generateMIDIParams.m_midiTrack = midiTrack;
            generateMIDIParams.m_staffN = staves->first;
            generateMIDIParams.m_transSemi = transSemi;
            generateMIDIParams.m_currentTempo = tempo;
            generateMIDIParams.m_deferredNotes = initMIDIParams.m_deferredNotes;
            generateMIDIParams.m_cueExclusion = this->GetOptions()->m_midiNoCue.GetValue();

            // LogDebug("Exporting track %d ----------------", midiTrack);
            this->Process(&generateMIDI, &generateMIDIParams, &generateMIDIEnd, &filters);
        }
    }
}

bool Doc::ExportTimemap(std::string &output, bool includeRests, bool includeMeasures)
{
    if (!Doc::HasTimemap()) {
        // generate MIDI timemap before progressing
        CalculateTimemap();
    }
    if (!Doc::HasTimemap()) {
        LogWarning("Calculation of MIDI timemap failed, not exporting MidiFile.");
        output = "";
        return false;
    }
    Timemap timemap;
    Functor generateTimemap(&Object::GenerateTimemap);
    GenerateTimemapParams generateTimemapParams(&timemap, &generateTimemap);
    generateTimemapParams.m_cueExclusion = this->GetOptions()->m_midiNoCue.GetValue();
    this->Process(&generateTimemap, &generateTimemapParams);

    timemap.ToJson(output, includeRests, includeMeasures);

    return true;
}

bool Doc::ExportFeatures(std::string &output, const std::string &options)
{
    if (!Doc::HasTimemap()) {
        // generate MIDI timemap before progressing
        CalculateTimemap();
    }
    if (!Doc::HasTimemap()) {
        LogWarning("Calculation of MIDI timemap failed, not exporting MidiFile.");
        output = "";
        return false;
    }
    FeatureExtractor extractor(options);
    Functor generateFeatures(&Object::GenerateFeatures);
    GenerateFeaturesParams generateFeaturesParams(this, &extractor);
    this->Process(&generateFeatures, &generateFeaturesParams);
    extractor.ToJson(output);

    return true;
}

void Doc::PrepareData()
{
    /************ Reset and initialization ************/
    if (m_dataPreparationDone) {
        Functor resetData(&Object::ResetData);
        this->Process(&resetData, NULL);
    }
    Functor prepareDataInitialization(&Object::PrepareDataInitialization);
    PrepareDataInitializationParams prepareDataInitializationParams(&prepareDataInitialization, this);
    this->Process(&prepareDataInitialization, &prepareDataInitializationParams);

    /************ Store default durations ************/

    Functor prepareDuration(&Object::PrepareDuration);
    PrepareDurationParams prepareDurationParams(&prepareDuration);
    this->Process(&prepareDuration, &prepareDurationParams);

    /************ Resolve @startid / @endid ************/

    // Try to match all spanning elements (slur, tie, etc) by processing backwards
    PrepareTimeSpanningParams prepareTimeSpanningParams;
    Functor prepareTimeSpanning(&Object::PrepareTimeSpanning);
    Functor prepareTimeSpanningEnd(&Object::PrepareTimeSpanningEnd);
    this->Process(
        &prepareTimeSpanning, &prepareTimeSpanningParams, &prepareTimeSpanningEnd, NULL, UNLIMITED_DEPTH, BACKWARD);

    // First we try backwards because normally the spanning elements are at the end of
    // the measure. However, in some case, one (or both) end points will appear afterwards
    // in the encoding. For these, the previous iteration will not have resolved the link and
    // the spanning elements will remain in the timeSpanningElements array. We try again forwards
    // but this time without filling the list (that is only will the remaining elements)
    if (!prepareTimeSpanningParams.m_timeSpanningInterfaces.empty()) {
        prepareTimeSpanningParams.m_fillList = false;
        this->Process(&prepareTimeSpanning, &prepareTimeSpanningParams);
    }

    // Display warning if some elements were not matched
    const size_t unmatchedElements = std::count_if(prepareTimeSpanningParams.m_timeSpanningInterfaces.cbegin(),
        prepareTimeSpanningParams.m_timeSpanningInterfaces.cend(),
        [](const ListOfSpanningInterOwnerPairs::value_type &entry) {
            return (entry.first->HasStartid() && entry.first->HasEndid());
        });
    if (unmatchedElements > 0) {
        LogWarning("%d time spanning element(s) with startid and endid could not be matched.", unmatchedElements);
    }

    /************ Resolve @startid (only) ************/

    // Resolve <reh> elements first, since they can be encoded without @startid or @tstamp, but we need one internally
    // for placement
    Functor prepareRehPosition(&Object::PrepareRehPosition);
    this->Process(&prepareRehPosition, NULL);

    // Try to match all time pointing elements (tempo, fermata, etc) by processing backwards
    PrepareTimePointingParams prepareTimePointingParams;
    Functor prepareTimePointing(&Object::PrepareTimePointing);
    Functor prepareTimePointingEnd(&Object::PrepareTimePointingEnd);
    this->Process(
        &prepareTimePointing, &prepareTimePointingParams, &prepareTimePointingEnd, NULL, UNLIMITED_DEPTH, BACKWARD);

    /************ Resolve @tstamp / tstamp2 ************/

    // Now try to match the @tstamp and @tstamp2 attributes.
    PrepareTimestampsParams prepareTimestampsParams;
    Functor prepareTimestamps(&Object::PrepareTimestamps);
    Functor prepareTimestampsEnd(&Object::PrepareTimestampsEnd);
    this->Process(&prepareTimestamps, &prepareTimestampsParams, &prepareTimestampsEnd);

    // If some are still there, then it is probably an issue in the encoding
    if (!prepareTimestampsParams.m_timeSpanningInterfaces.empty()) {
        LogWarning("%d time spanning element(s) with timestamps could not be matched.",
            prepareTimestampsParams.m_timeSpanningInterfaces.size());
    }

    /************ Resolve linking (@next) ************/

    // Try to match all pointing elements using @next, @sameas and @stem.sameas
    PrepareLinkingParams prepareLinkingParams;
    Functor prepareLinking(&Object::PrepareLinking);
    this->Process(&prepareLinking, &prepareLinkingParams);

    // If we have some left process again backward
    if (!prepareLinkingParams.m_sameasIDPairs.empty() || !prepareLinkingParams.m_stemSameasIDPairs.empty()) {
        prepareLinkingParams.m_fillList = false;
        this->Process(&prepareLinking, &prepareLinkingParams, NULL, NULL, UNLIMITED_DEPTH, BACKWARD);
    }

    // If some are still there, then it is probably an issue in the encoding
    if (!prepareLinkingParams.m_nextIDPairs.empty()) {
        LogWarning("%d element(s) with a @next could match the target", prepareLinkingParams.m_nextIDPairs.size());
    }
    if (!prepareLinkingParams.m_sameasIDPairs.empty()) {
        LogWarning("%d element(s) with a @sameas could match the target", prepareLinkingParams.m_sameasIDPairs.size());
    }
    if (!prepareLinkingParams.m_stemSameasIDPairs.empty()) {
        LogWarning("%d element(s) with a @stem.sameas could match the target",
            prepareLinkingParams.m_stemSameasIDPairs.size());
    }

    /************ Resolve @plist ************/

    // Try to match all pointing elements using @plist
    PreparePlistParams preparePlistParams;
    Functor preparePlist(&Object::PreparePlist);
    this->Process(&preparePlist, &preparePlistParams);

    // Process plist after all pairs has been collected
    if (!preparePlistParams.m_interfaceIDTuples.empty()) {
        preparePlistParams.m_fillList = false;
        Functor processPlist(&Object::PrepareProcessPlist);
        this->Process(&processPlist, &preparePlistParams);

        for (const auto &[plistInterface, id, objectReference] : preparePlistParams.m_interfaceIDTuples) {
            plistInterface->SetRef(objectReference);
        }
        preparePlistParams.m_interfaceIDTuples.clear();
    }

    // If some are still there, then it is probably an issue in the encoding
    if (!preparePlistParams.m_interfaceIDTuples.empty()) {
        LogWarning(
            "%d element(s) with a @plist could not match the target", preparePlistParams.m_interfaceIDTuples.size());
    }

    /************ Resolve cross staff ************/

    // Prepare the cross-staff pointers
    PrepareCrossStaffParams prepareCrossStaffParams;
    Functor prepareCrossStaff(&Object::PrepareCrossStaff);
    Functor prepareCrossStaffEnd(&Object::PrepareCrossStaffEnd);
    this->Process(&prepareCrossStaff, &prepareCrossStaffParams, &prepareCrossStaffEnd);

    /************ Resolve beamspan elements ***********/

    FunctorDocParams functorDocParams(this);
    Functor prepareBeamSpanElements(&Object::PrepareBeamSpanElements);
    this->Process(&prepareBeamSpanElements, &functorDocParams);

    /************ Prepare processing by staff/layer/verse ************/

    // We need to populate processing lists for processing the document by Layer (for matching @tie) and
    // by Verse (for matching syllable connectors)
    InitProcessingListsParams initProcessingListsParams;
    // Alternate solution with StaffN_LayerN_VerseN_t (see also Verse::PrepareData)
    // StaffN_LayerN_VerseN_t staffLayerVerseTree;
    // params.push_back(&staffLayerVerseTree);

    // We first fill a tree of ints with [staff/layer] and [staff/layer/verse] numbers (@n) to be processed
    // LogElapsedTimeStart();
    Functor initProcessingLists(&Object::InitProcessingLists);
    this->Process(&initProcessingLists, &initProcessingListsParams);

    // The tree is used to process each staff/layer/verse separately
    // For this, we use an array of AttNIntegerComparison that looks for each object if it is of the type
    // and with @n specified

    IntTree_t::iterator staves;
    IntTree_t::iterator layers;
    IntTree_t::iterator verses;

    /************ Resolve some pointers by layer ************/

    Filters filters;
    for (staves = initProcessingListsParams.m_layerTree.child.begin();
         staves != initProcessingListsParams.m_layerTree.child.end(); ++staves) {
        for (layers = staves->second.child.begin(); layers != staves->second.child.end(); ++layers) {
            filters.Clear();
            // Create ad comparison object for each type / @n
            AttNIntegerComparison matchStaff(STAFF, staves->first);
            AttNIntegerComparison matchLayer(LAYER, layers->first);
            filters.Add(&matchStaff);
            filters.Add(&matchLayer);

            PreparePointersByLayerParams preparePointersByLayerParams;
            Functor preparePointersByLayer(&Object::PreparePointersByLayer);
            this->Process(&preparePointersByLayer, &preparePointersByLayerParams, NULL, &filters);
        }
    }

    /************ Resolve delayed turns ************/

    PrepareDelayedTurnsParams prepareDelayedTurnsParams;
    Functor prepareDelayedTurns(&Object::PrepareDelayedTurns);
    this->Process(&prepareDelayedTurns, &prepareDelayedTurnsParams);

    if (!prepareDelayedTurnsParams.m_delayedTurns.empty()) {
        prepareDelayedTurnsParams.m_initMap = false;
        for (staves = initProcessingListsParams.m_layerTree.child.begin();
             staves != initProcessingListsParams.m_layerTree.child.end(); ++staves) {
            for (layers = staves->second.child.begin(); layers != staves->second.child.end(); ++layers) {
                filters.Clear();
                // Create ad comparison object for each type / @n
                AttNIntegerComparison matchStaff(STAFF, staves->first);
                AttNIntegerComparison matchLayer(LAYER, layers->first);
                filters.Add(&matchStaff);
                filters.Add(&matchLayer);

                prepareDelayedTurnsParams.m_currentTurn = NULL;
                prepareDelayedTurnsParams.m_previousElement = NULL;

                this->Process(&prepareDelayedTurns, &prepareDelayedTurnsParams, NULL, &filters);
            }
        }
    }

    /************ Resolve lyric connectors ************/

    // Same for the lyrics, but Verse by Verse since Syl are TimeSpanningInterface elements for handling connectors
    for (staves = initProcessingListsParams.m_verseTree.child.begin();
         staves != initProcessingListsParams.m_verseTree.child.end(); ++staves) {
        for (layers = staves->second.child.begin(); layers != staves->second.child.end(); ++layers) {
            for (verses = layers->second.child.begin(); verses != layers->second.child.end(); ++verses) {
                // std::cout << staves->first << " => " << layers->first << " => " << verses->first << '\n';
                filters.Clear();
                // Create ad comparison object for each type / @n
                AttNIntegerComparison matchStaff(STAFF, staves->first);
                AttNIntegerComparison matchLayer(LAYER, layers->first);
                AttNIntegerComparison matchVerse(VERSE, verses->first);
                filters.Add(&matchStaff);
                filters.Add(&matchLayer);
                filters.Add(&matchVerse);

                // The first pass sets m_drawingFirstNote and m_drawingLastNote for each syl
                // m_drawingLastNote is set only if the syl has a forward connector
                PrepareLyricsParams prepareLyricsParams;
                Functor prepareLyrics(&Object::PrepareLyrics);
                Functor prepareLyricsEnd(&Object::PrepareLyricsEnd);
                this->Process(&prepareLyrics, &prepareLyricsParams, &prepareLyricsEnd, &filters);
            }
        }
    }

    /************ Fill control event spanning ************/

    // Once <slur>, <ties> and @ties are matched but also syl connectors, we need to set them as running
    // TimeSpanningInterface to each staff they are extended. This does not need to be done staff by staff because we
    // can just check the staff->GetN to see where we are (see Staff::PrepareStaffCurrentTimeSpanning)
    PrepareStaffCurrentTimeSpanningParams fillStaffCurrentTimeSpanningParams;
    Functor fillStaffCurrentTimeSpanning(&Object::PrepareStaffCurrentTimeSpanning);
    Functor fillStaffCurrentTimeSpanningEnd(&Object::PrepareStaffCurrentTimeSpanningEnd);
    this->Process(&fillStaffCurrentTimeSpanning, &fillStaffCurrentTimeSpanningParams, &fillStaffCurrentTimeSpanningEnd);

    // Something must be wrong in the encoding because a TimeSpanningInterface was left open
    if (!fillStaffCurrentTimeSpanningParams.m_timeSpanningElements.empty()) {
        LogDebug("%d time spanning elements could not be set as running",
            fillStaffCurrentTimeSpanningParams.m_timeSpanningElements.size());
    }

    /************ Resolve mRpt ************/

    // Process by staff for matching mRpt elements and setting the drawing number
    for (staves = initProcessingListsParams.m_layerTree.child.begin();
         staves != initProcessingListsParams.m_layerTree.child.end(); ++staves) {
        for (layers = staves->second.child.begin(); layers != staves->second.child.end(); ++layers) {
            filters.Clear();
            // Create ad comparison object for each type / @n
            AttNIntegerComparison matchStaff(STAFF, staves->first);
            AttNIntegerComparison matchLayer(LAYER, layers->first);
            filters.Add(&matchStaff);
            filters.Add(&matchLayer);

            // We set multiNumber to NONE for indicated we need to look at the staffDef when reaching the first staff
            PrepareRptParams prepareRptParams(this);
            Functor prepareRpt(&Object::PrepareRpt);
            this->Process(&prepareRpt, &prepareRptParams, NULL, &filters);
        }
    }

    /************ Resolve endings ************/

    // Prepare the endings (pointers to the measure after and before the boundaries
    PrepareMilestonesParams prepareEndingsParams;
    Functor prepareEndings(&Object::PrepareMilestones);
    this->Process(&prepareEndings, &prepareEndingsParams);

    /************ Resolve floating groups for vertical alignment ************/

    // Prepare the floating drawing groups
    PrepareFloatingGrpsParams prepareFloatingGrpsParams(this);
    Functor prepareFloatingGrps(&Object::PrepareFloatingGrps);
    Functor prepareFloatingGrpsEnd(&Object::PrepareFloatingGrpsEnd);
    this->Process(&prepareFloatingGrps, &prepareFloatingGrpsParams, &prepareFloatingGrpsEnd);

    /************ Resolve cue size ************/

    // Prepare the drawing cue size
    Functor prepareCueSize(&Object::PrepareCueSize);
    this->Process(&prepareCueSize, NULL);

    /************ Instanciate LayerElement parts (stemp, flag, dots, etc) ************/

    Functor prepareLayerElementParts(&Object::PrepareLayerElementParts);
    this->Process(&prepareLayerElementParts, NULL);

    /*
    // Alternate solution with StaffN_LayerN_VerseN_t
    StaffN_LayerN_VerseN_t::iterator staves;
    LayerN_VerserN_t::iterator layers;
    VerseN_t::iterator verses;
    std::vector<AttComparison*> filters;
    for (staves = staffLayerVerseTree.begin(); staves != staffLayerVerseTree.end(); ++staves) {
        for (layers = staves->second.begin(); layers != staves->second.end(); ++layers) {
            for (verses= layers->second.begin(); verses != layers->second.end(); ++verses) {
                std::cout << staves->first << " => " << layers->first << " => " << verses->first << '\n';
                filters.clear();
                AttNIntegerComparison matchStaff(&typeid(Staff), staves->first);
                AttNIntegerComparison matchLayer(&typeid(Layer), layers->first);
                AttNIntegerComparison matchVerse(&typeid(Verse), verses->first);
                filters.push_back(&matchStaff);
                filters.push_back(&matchLayer);
                filters.push_back(&matchVerse);

                FunctorParams paramsLyrics;
                Functor prepareLyrics(&Object::PrepareLyrics);
                this->Process(&prepareLyrics, paramsLyrics, NULL, &filters);
            }
        }
    }
    */

    /************ Add default syl for syllables (if applicable) ************/
    ListOfObjects syllables = this->FindAllDescendantsByType(SYLLABLE);
    for (auto it = syllables.begin(); it != syllables.end(); ++it) {
        Syllable *syllable = dynamic_cast<Syllable *>(*it);
        syllable->MarkupAddSyl();
    }

    /************ Resolve @facs ************/
    if (this->GetType() == Facs) {
        // Associate zones with elements
        PrepareFacsimileParams prepareFacsimileParams(this->GetFacsimile());
        Functor prepareFacsimile(&Object::PrepareFacsimile);
        this->Process(&prepareFacsimile, &prepareFacsimileParams);

        // Add default syl zone if one is not present.
        for (auto &it : prepareFacsimileParams.m_zonelessSyls) {
            Syl *syl = vrv_cast<Syl *>(it);
            assert(syl);
            syl->CreateDefaultZone(this);
        }
    }

    Functor scoreDefSetGrpSym(&Object::ScoreDefSetGrpSym);
    this->GetCurrentScoreDef()->Process(&scoreDefSetGrpSym, NULL);

    // LogElapsedTimeEnd ("Preparing drawing");

    m_dataPreparationDone = true;
}

void Doc::ScoreDefSetCurrentDoc(bool force)
{
    if (m_currentScoreDefDone && !force) {
        return;
    }

    if (m_currentScoreDefDone) {
        Functor scoreDefUnsetCurrent(&Object::ScoreDefUnsetCurrent);
        ScoreDefUnsetCurrentParams scoreDefUnsetCurrentParams(&scoreDefUnsetCurrent);
        this->Process(&scoreDefUnsetCurrent, &scoreDefUnsetCurrentParams);
    }

    // First we need to set Page::m_score and Page::m_scoreEnd
    // We do it by going BACKWARD, with a depth limit of 3 (we want to hit the Score elements)
    // The Doc::m_currentScore is set by Object::Process
    // The Page::m_score in Page::ScoreDefSetCurrentPageEnd
    Functor scoreDefSetCurrentPage(&Object::ScoreDefSetCurrentPage);
    Functor scoreDefSetCurrentPageEnd(&Object::ScoreDefSetCurrentPageEnd);
    FunctorDocParams scoreDefSetCurrentPageParams(this);
    this->Process(
        &scoreDefSetCurrentPage, &scoreDefSetCurrentPageParams, &scoreDefSetCurrentPageEnd, NULL, 3, BACKWARD);
    // Do it again FORWARD to set Page::m_scoreEnd - relies on Page::m_score not being NULL
    this->Process(&scoreDefSetCurrentPage, &scoreDefSetCurrentPageParams, &scoreDefSetCurrentPageEnd, NULL, 3, FORWARD);

    // ScoreDef upcomingScoreDef;
    Functor scoreDefSetCurrent(&Object::ScoreDefSetCurrent);
    ScoreDefSetCurrentParams scoreDefSetCurrentParams(this, &scoreDefSetCurrent);
    this->Process(&scoreDefSetCurrent, &scoreDefSetCurrentParams);

    this->ScoreDefSetGrpSymDoc();

    m_currentScoreDefDone = true;
}

void Doc::ScoreDefOptimizeDoc()
{
    Functor scoreDefOptimize(&Object::ScoreDefOptimize);
    Functor scoreDefOptimizeEnd(&Object::ScoreDefOptimizeEnd);
    ScoreDefOptimizeParams scoreDefOptimizeParams(this, &scoreDefOptimize, &scoreDefOptimizeEnd);

    this->Process(&scoreDefOptimize, &scoreDefOptimizeParams, &scoreDefOptimizeEnd);

    this->ScoreDefSetGrpSymDoc();
}

void Doc::ScoreDefSetGrpSymDoc()
{
    // Group symbols need to be resolved using scoreDef, since there might be @starid/@endid attributes that determine
    // their positioning
    Functor scoreDefSetGrpSym(&Object::ScoreDefSetGrpSym);
    // this->GetCurrentScoreDef()->Process(&scoreDefSetGrpSym, NULL);
    ScoreDefSetGrpSymParams scoreDefSetGrpSymParams(&scoreDefSetGrpSym);
    this->Process(&scoreDefSetGrpSym, &scoreDefSetGrpSymParams);
}

void Doc::CastOffDoc()
{
    Doc::CastOffDocBase(false, false);
}

void Doc::CastOffLineDoc()
{
    Doc::CastOffDocBase(true, false);
}

void Doc::CastOffSmartDoc()
{
    Doc::CastOffDocBase(false, false, true);
}

void Doc::CastOffDocBase(bool useSb, bool usePb, bool smart)
{
    Pages *pages = this->GetPages();
    assert(pages);

    if (this->IsCastOff()) {
        LogDebug("Document is already cast off");
        return;
    }

    std::list<Score *> scores = this->GetScores();
    assert(!scores.empty());

    this->ScoreDefSetCurrentDoc();

    Page *unCastOffPage = this->SetDrawingPage(0);
    assert(unCastOffPage);

    // Check if the the horizontal layout is cached by looking at the first measure
    // The cache is not set the first time, or can be reset by Doc::UnCastOffDoc
    Measure *firstMeasure = vrv_cast<Measure *>(unCastOffPage->FindDescendantByType(MEASURE));
    if (!firstMeasure || !firstMeasure->HasCachedHorizontalLayout()) {
        // LogDebug("Performing the horizontal layout");
        unCastOffPage->LayOutHorizontally();
        unCastOffPage->LayOutHorizontallyWithCache();
    }
    else {
        unCastOffPage->LayOutHorizontallyWithCache(true);
    }

    Page *castOffSinglePage = new Page();

    System *leftoverSystem = NULL;
    if (useSb && !usePb && !smart) {
        CastOffEncodingParams castOffEncodingParams(this, castOffSinglePage, false);
        Functor castOffEncoding(&Object::CastOffEncoding);
        unCastOffPage->Process(&castOffEncoding, &castOffEncodingParams);
    }
    else {
        CastOffSystemsParams castOffSystemsParams(castOffSinglePage, this, smart);
        castOffSystemsParams.m_systemWidth = m_drawingPageContentWidth;

        Functor castOffSystems(&Object::CastOffSystems);
        Functor castOffSystemsEnd(&Object::CastOffSystemsEnd);
        unCastOffPage->Process(&castOffSystems, &castOffSystemsParams, &castOffSystemsEnd);
        leftoverSystem = castOffSystemsParams.m_leftoverSystem;
    }
    // We can now detach and delete the old content page
    pages->DetachChild(0);
    assert(unCastOffPage && !unCastOffPage->GetParent());
    delete unCastOffPage;
    unCastOffPage = NULL;

    // Store the cast off system widths => these are used to adjust the horizontal spacing
    // for a given duration during page layout
    AlignMeasuresParams alignMeasuresParams(this);
    alignMeasuresParams.m_storeCastOffSystemWidths = true;
    Functor alignMeasures(&Object::AlignMeasures);
    Functor alignMeasuresEnd(&Object::AlignMeasuresEnd);
    castOffSinglePage->Process(&alignMeasures, &alignMeasuresParams, &alignMeasuresEnd);

    // Replace it with the castOffSinglePage
    pages->AddChild(castOffSinglePage);
    this->ResetDataPage();
    this->SetDrawingPage(0);

    bool optimize = false;
    for (auto const score : scores) {
        if (score->ScoreDefNeedsOptimization(m_options->m_condense.GetValue())) {
            optimize = true;
            break;
        }
    }

    // Reset the scoreDef at the beginning of each system
    this->ScoreDefSetCurrentDoc(true);
    if (optimize) {
        this->ScoreDefOptimizeDoc();
    }

    // Here we redo the alignment because of the new scoreDefs
    // Because of the new scoreDef, we need to reset cached drawingX
    castOffSinglePage->ResetCachedDrawingX();
    castOffSinglePage->LayOutVertically();

    // Detach the contentPage in order to be able call CastOffRunningElements
    pages->DetachChild(0);
    assert(castOffSinglePage && !castOffSinglePage->GetParent());
    this->ResetDataPage();

    for (auto const score : scores) {
        score->CalcRunningElementHeight(this);
    }

    Page *castOffFirstPage = new Page();
    CastOffPagesParams castOffPagesParams(castOffSinglePage, this, castOffFirstPage);
    castOffPagesParams.m_pageHeight = this->m_drawingPageContentHeight;
    castOffPagesParams.m_leftoverSystem = leftoverSystem;

    Functor castOffPages(&Object::CastOffPages);
    Functor castOffPagesEnd(&Object::CastOffPagesEnd);
    pages->AddChild(castOffFirstPage);
    castOffSinglePage->Process(&castOffPages, &castOffPagesParams, &castOffPagesEnd);
    delete castOffSinglePage;

    this->ScoreDefSetCurrentDoc(true);
    if (optimize) {
        this->ScoreDefOptimizeDoc();
    }

    m_isCastOff = true;
}

void Doc::UnCastOffDoc(bool resetCache)
{
    if (!this->IsCastOff()) {
        LogDebug("Document is not cast off");
        return;
    }

    Pages *pages = this->GetPages();
    assert(pages);

    Page *unCastOffPage = new Page();
    UnCastOffParams unCastOffParams(unCastOffPage);
    unCastOffParams.m_resetCache = resetCache;

    Functor unCastOff(&Object::UnCastOff);
    this->Process(&unCastOff, &unCastOffParams);

    pages->ClearChildren();

    pages->AddChild(unCastOffPage);

    // LogDebug("ContinuousLayout: %d pages", this->GetChildCount());

    // We need to reset the drawing page to NULL
    // because idx will still be 0 but contentPage is dead!
    this->ResetDataPage();
    this->ScoreDefSetCurrentDoc(true);

    m_isCastOff = false;
}

void Doc::CastOffEncodingDoc()
{
    if (this->IsCastOff()) {
        LogDebug("Document is already cast off");
        return;
    }

    this->ScoreDefSetCurrentDoc();

    Pages *pages = this->GetPages();
    assert(pages);

    Page *unCastOffPage = this->SetDrawingPage(0);
    assert(unCastOffPage);
    unCastOffPage->ResetAligners();

    // Detach the contentPage
    pages->DetachChild(0);
    assert(unCastOffPage && !unCastOffPage->GetParent());

    Page *castOffFirstPage = new Page();
    pages->AddChild(castOffFirstPage);

    CastOffEncodingParams castOffEncodingParams(this, castOffFirstPage);

    Functor castOffEncoding(&Object::CastOffEncoding);
    unCastOffPage->Process(&castOffEncoding, &castOffEncodingParams);
    delete unCastOffPage;

    // We need to reset the drawing page to NULL
    // because idx will still be 0 but contentPage is dead!
    this->ResetDataPage();
    this->ScoreDefSetCurrentDoc(true);

    // Optimize the doc if one of the score requires optimization
    for (auto const score : this->GetScores()) {
        if (score->ScoreDefNeedsOptimization(m_options->m_condense.GetValue())) {
            this->ScoreDefOptimizeDoc();
            break;
        }
    }

    m_isCastOff = true;
}

void Doc::InitSelectionDoc(DocSelection &selection, bool resetCache)
{
    // No new selection to apply;
    if (!selection.m_isPending) return;

    if (this->HasSelection()) {
        this->ResetSelectionDoc(resetCache);
    }

    selection.Set(this);

    if (!this->HasSelection()) return;

    assert(!m_selectionPreceeding && !m_selectionFollowing);

    if (this->IsCastOff()) this->UnCastOffDoc();

    Pages *pages = this->GetPages();
    assert(pages);

    this->ScoreDefSetCurrentDoc();

    Page *unCastOffPage = this->SetDrawingPage(0);

    // Make sure we have global slurs curve dir
    unCastOffPage->ResetAligners();

    // We can now detach and delete the old content page
    pages->DetachChild(0);
    assert(unCastOffPage);

    Page *selectionFirstPage = new Page();
    pages->AddChild(selectionFirstPage);

    CastOffToSelectionParams castOffToSelectionParams(selectionFirstPage, this, m_selectionStart, m_selectionEnd);
    Functor castOffToSelection(&Object::CastOffToSelection);

    unCastOffPage->Process(&castOffToSelection, &castOffToSelectionParams);

    delete unCastOffPage;

    this->ResetDataPage();
    this->ScoreDefSetCurrentDoc(true);

    if (pages->GetChildCount() < 2) {
        LogWarning("Selection could not be made");
        m_selectionStart = "";
        m_selectionEnd = "";
        return;
    }
    else if (pages->GetChildCount() == 2) {
        LogWarning("Selection end '%s' could not be found", m_selectionEnd.c_str());
        // Add an empty page to make it work
        pages->AddChild(new Page());
    }

    this->ReactivateSelection(true);
}

void Doc::ResetSelectionDoc(bool resetCache)
{
    assert(m_selectionPreceeding && m_selectionFollowing);

    m_selectionStart = "";
    m_selectionEnd = "";

    if (this->IsCastOff()) this->UnCastOffDoc();

    this->DeactiveateSelection();

    this->m_isCastOff = true;
    this->UnCastOffDoc(resetCache);
}

bool Doc::HasSelection() const
{
    return (!m_selectionStart.empty() && !m_selectionEnd.empty());
}

void Doc::DeactiveateSelection()
{
    Pages *pages = this->GetPages();
    assert(pages);

    Page *selectionPage = vrv_cast<Page *>(pages->GetChild(0));
    assert(selectionPage);
    // We need to delete the selection scoreDef
    Score *selectionScore = vrv_cast<Score *>(selectionPage->FindDescendantByType(SCORE));
    assert(selectionScore);
    if (selectionScore->GetLabel() != "[selectionScore]") LogError("Deleting wrong score element. Something is wrong");
    selectionPage->DeleteChild(selectionScore);

    m_selectionPreceeding->SetParent(pages);
    pages->InsertChild(m_selectionPreceeding, 0);
    pages->AddChild(m_selectionFollowing);

    m_selectionPreceeding = NULL;
    m_selectionFollowing = NULL;
}

void Doc::ReactivateSelection(bool resetAligners)
{
    Pages *pages = this->GetPages();
    assert(pages);

    const int lastPage = pages->GetChildCount() - 1;
    assert(lastPage > 1);

    Page *selectionPage = vrv_cast<Page *>(pages->GetChild(1));
    System *system = vrv_cast<System *>(selectionPage->FindDescendantByType(SYSTEM));
    // Add a selection scoreDef based on the current drawing system scoreDef
    Score *selectionScore = new Score();
    selectionScore->SetLabel("[selectionScore]");
    *selectionScore->GetScoreDef() = *system->GetDrawingScoreDef();
    // Use the drawing values as actual scoreDef
    selectionScore->GetScoreDef()->ResetFromDrawingValues();
    selectionScore->SetParent(selectionPage);
    selectionPage->InsertChild(selectionScore, 0);

    m_selectionPreceeding = vrv_cast<Page *>(pages->GetChild(0));
    // Reset the aligners because data will be accessed when rendering control events outside the selection
    if (resetAligners && m_selectionPreceeding->FindDescendantByType(MEASURE)) {
        this->SetDrawingPage(0);
        m_selectionPreceeding->ResetAligners();
    }

    m_selectionFollowing = vrv_cast<Page *>(pages->GetChild(lastPage));
    // Same for the following content
    if (resetAligners && m_selectionFollowing->FindDescendantByType(MEASURE)) {
        this->SetDrawingPage(2);
        m_selectionFollowing->ResetAligners();
    }

    // Detach the preceeding and following page
    pages->DetachChild(lastPage);
    pages->DetachChild(0);
    // Make sure we do not point to page moved out of the selection
    this->m_drawingPage = NULL;
}

void Doc::ConvertToPageBasedDoc()
{
    Pages *pages = new Pages();
    Page *page = new Page();
    pages->AddChild(page);

    ConvertToPageBasedParams convertToPageBasedParams(page);
    Functor convertToPageBased(&Object::ConvertToPageBased);
    Functor convertToPageBasedEnd(&Object::ConvertToPageBasedEnd);
    this->Process(&convertToPageBased, &convertToPageBasedParams, &convertToPageBasedEnd);

    this->ClearRelinquishedChildren();
    assert(this->GetChildCount() == 0);

    this->AddChild(pages);

    this->ResetDataPage();
}

void Doc::ConvertToCastOffMensuralDoc(bool castOff)
{
    if (!m_isMensuralMusicOnly) return;

    // Do not convert transcription files
    if (this->GetType() == Transcription) return;

    // Do not convert facs files
    if (this->GetType() == Facs) return;

    // We are converting to measure music in a definite way
    if (this->GetOptions()->m_mensuralToMeasure.GetValue()) {
        m_isMensuralMusicOnly = false;
    }

    // Make sure the document is not cast-off
    this->UnCastOffDoc();

    this->ScoreDefSetCurrentDoc();

    Page *contentPage = this->SetDrawingPage(0);
    assert(contentPage);

    contentPage->LayOutHorizontally();

    ListOfObjects systems = contentPage->FindAllDescendantsByType(SYSTEM, false, 1);
    for (const auto item : systems) {
        System *system = vrv_cast<System *>(item);
        assert(system);
        if (castOff) {
            System *convertedSystem = new System();
            system->ConvertToCastOffMensuralSystem(this, convertedSystem);
            contentPage->ReplaceChild(system, convertedSystem);
            delete system;
        }
        else {
            system->ConvertToUnCastOffMensuralSystem();
        }
    }

    this->PrepareData();

    // We need to reset the drawing page to NULL
    // because idx will still be 0 but contentPage is dead!
    this->ResetDataPage();
    this->ScoreDefSetCurrentDoc(true);
}

void Doc::ConvertMarkupDoc(bool permanent)
{
    if (m_markup == MARKUP_DEFAULT) return;

    LogMessage("Converting markup...");

    if (m_markup & MARKUP_GRACE_ATTRIBUTE) {
    }

    if (m_markup & MARKUP_ARTIC_MULTIVAL) {
        LogMessage("Converting artic markup...");
        ConvertMarkupArticParams convertMarkupArticParams;
        Functor convertMarkupArtic(&Object::ConvertMarkupArtic);
        Functor convertMarkupArticEnd(&Object::ConvertMarkupArticEnd);
        this->Process(&convertMarkupArtic, &convertMarkupArticParams, &convertMarkupArticEnd);
    }

    if ((m_markup & MARKUP_ANALYTICAL_FERMATA) || (m_markup & MARKUP_ANALYTICAL_TIE)) {
        LogMessage("Converting analytical markup...");
        /************ Prepare processing by staff/layer/verse ************/

        // We need to populate processing lists for processing the document by Layer (for matching @tie) and
        // by Verse (for matching syllable connectors)
        InitProcessingListsParams initProcessingListsParams;

        // We first fill a tree of ints with [staff/layer] and [staff/layer/verse] numbers (@n) to be processed
        Functor initProcessingLists(&Object::InitProcessingLists);
        this->Process(&initProcessingLists, &initProcessingListsParams);

        IntTree_t::iterator staves;
        IntTree_t::iterator layers;

        /************ Resolve ties ************/

        // Process by layer for matching @tie attribute - we process notes and chords, looking at
        // GetTie values and pitch and oct for matching notes
        Filters filters;
        for (staves = initProcessingListsParams.m_layerTree.child.begin();
             staves != initProcessingListsParams.m_layerTree.child.end(); ++staves) {
            for (layers = staves->second.child.begin(); layers != staves->second.child.end(); ++layers) {
                filters.Clear();
                // Create ad comparison object for each type / @n
                AttNIntegerComparison matchStaff(STAFF, staves->first);
                AttNIntegerComparison matchLayer(LAYER, layers->first);
                filters.Add(&matchStaff);
                filters.Add(&matchLayer);

                ConvertMarkupAnalyticalParams convertMarkupAnalyticalParams(permanent);
                Functor convertMarkupAnalytical(&Object::ConvertMarkupAnalytical);
                Functor convertMarkupAnalyticalEnd(&Object::ConvertMarkupAnalyticalEnd);
                this->Process(
                    &convertMarkupAnalytical, &convertMarkupAnalyticalParams, &convertMarkupAnalyticalEnd, &filters);

                // After having processed one layer, we check if we have open ties - if yes, we
                // must reset them and they will be ignored.
                if (!convertMarkupAnalyticalParams.m_currentNotes.empty()) {
                    std::vector<Note *>::iterator iter;
                    for (iter = convertMarkupAnalyticalParams.m_currentNotes.begin();
                         iter != convertMarkupAnalyticalParams.m_currentNotes.end(); ++iter) {
                        LogWarning("Unable to match @tie of note '%s', skipping it", (*iter)->GetID().c_str());
                    }
                }
            }
        }
    }

    if (m_markup & MARKUP_SCOREDEF_DEFINITIONS) {
        LogMessage("Converting scoreDef markup...");
        Functor convertMarkupScoreDef(&Object::ConvertMarkupScoreDef);
        Functor convertMarkupScoreDefEnd(&Object::ConvertMarkupScoreDefEnd);
        ConvertMarkupScoreDefParams convertMarkupScoreDefParams(
            this, &convertMarkupScoreDef, &convertMarkupScoreDefEnd);
        this->Process(&convertMarkupScoreDef, &convertMarkupScoreDefParams, &convertMarkupScoreDefEnd);
    }
}

void Doc::TransposeDoc()
{
    Transposer transposer;
    transposer.SetBase600(); // Set extended chromatic alteration mode (allowing more than double sharps/flats)

    Functor transpose(&Object::Transpose);
    Functor transposeEnd(&Object::TransposeEnd);
    TransposeParams transposeParams(this, &transpose, &transposeEnd, &transposer);

    if (m_options->m_transposeSelectedOnly.GetValue() == false) {
        transpose.m_visibleOnly = false;
    }

    if (m_options->m_transpose.IsSet()) {
        // Transpose the entire document
        if (m_options->m_transposeMdiv.IsSet()) {
            LogWarning("\"%s\" is ignored when \"%s\" is set as well. Please use only one of the two options.",
                m_options->m_transposeMdiv.GetKey().c_str(), m_options->m_transpose.GetKey().c_str());
        }
        transposeParams.m_transposition = m_options->m_transpose.GetValue();
        this->Process(&transpose, &transposeParams, &transposeEnd);
    }
    else if (m_options->m_transposeMdiv.IsSet()) {
        // Transpose mdivs individually
        std::set<std::string> ids = m_options->m_transposeMdiv.GetKeys();
        for (const std::string &id : ids) {
            transposeParams.m_selectedMdivID = id;
            transposeParams.m_transposition = m_options->m_transposeMdiv.GetStrValue({ id });
            this->Process(&transpose, &transposeParams, &transposeEnd);
        }
    }

    if (m_options->m_transposeToSoundingPitch.GetValue()) {
        // Transpose to sounding pitch
        transposeParams.m_selectedMdivID = "";
        transposeParams.m_transposition = "";
        transposeParams.m_transposer->SetTransposition(0);
        transposeParams.m_transposeToSoundingPitch = true;
        this->Process(&transpose, &transposeParams, &transposeEnd);
    }
}

void Doc::ExpandExpansions()
{
    // Upon MEI import: use expansion ID, given by command line argument
    std::string expansionId = this->GetOptions()->m_expand.GetValue();
    if (expansionId.empty()) return;

    Expansion *start = dynamic_cast<Expansion *>(this->FindDescendantByID(expansionId));
    if (start == NULL) {
        LogMessage("Import MEI: expansion ID \"%s\" not found.", expansionId.c_str());
        return;
    }

    xsdAnyURI_List expansionList = start->GetPlist();
    xsdAnyURI_List existingList;
    m_expansionMap.Expand(expansionList, existingList, start);

    // save original/notated expansion as element in expanded MEI
    // Expansion *originalExpansion = new Expansion();
    // char rnd[35];
    // snprintf(rnd, 35, "expansion-notated-%016d", std::rand());
    // originalExpansion->SetID(rnd);

    // for (std::string ref : existingList) {
    //    originalExpansion->GetPlistInterface()->AddRef("#" + ref);
    //}

    // start->GetParent()->InsertAfter(start, originalExpansion);

    // std::cout << "[expand] original expansion xml:id=\"" << originalExpansion->GetID().c_str()
    //          << "\" plist={";
    // for (std::string s : existingList) std::cout << s.c_str() << ((s != existingList.back()) ? " " : "}.\n");

    // for (auto const &strVect : m_doc->m_expansionMap.m_map) { // DEBUG: display expansionMap on console
    //     std::cout << strVect.first << ": <";
    //     for (auto const &string : strVect.second)
    //        std::cout << string << ((string != strVect.second.back()) ? ", " : ">.\n");
    // }
}

bool Doc::HasPage(int pageIdx)
{
    Pages *pages = this->GetPages();
    assert(pages);
    return ((pageIdx >= 0) && (pageIdx < pages->GetChildCount()));
}

std::list<Score *> Doc::GetScores()
{
    std::list<Score *> scores;
    ListOfObjects objects = this->FindAllDescendantsByType(SCORE, false, 3);
    for (const auto object : objects) {
        Score *score = vrv_cast<Score *>(object);
        assert(score);
        scores.push_back(score);
    }
    return scores;
}

Pages *Doc::GetPages()
{
    return dynamic_cast<Pages *>(this->FindDescendantByType(PAGES));
}

const Pages *Doc::GetPages() const
{
    return dynamic_cast<const Pages *>(this->FindDescendantByType(PAGES));
}

int Doc::GetPageCount() const
{
    const Pages *pages = this->GetPages();
    return ((pages) ? pages->GetChildCount() : 0);
}

int Doc::GetGlyphHeight(wchar_t code, int staffSize, bool graceSize) const
{
    int x, y, w, h;
    const Resources &resources = this->GetResources();
    const Glyph *glyph = resources.GetGlyph(code);
    assert(glyph);
    glyph->GetBoundingBox(x, y, w, h);
    h = h * m_drawingSmuflFontSize / glyph->GetUnitsPerEm();
    if (graceSize) h = h * m_options->m_graceFactor.GetValue();
    h = h * staffSize / 100;
    return h;
}

int Doc::GetGlyphWidth(wchar_t code, int staffSize, bool graceSize) const
{
    int x, y, w, h;
    const Resources &resources = this->GetResources();
    const Glyph *glyph = resources.GetGlyph(code);
    assert(glyph);
    glyph->GetBoundingBox(x, y, w, h);
    w = w * m_drawingSmuflFontSize / glyph->GetUnitsPerEm();
    if (graceSize) w = w * m_options->m_graceFactor.GetValue();
    w = w * staffSize / 100;
    return w;
}

int Doc::GetGlyphAdvX(wchar_t code, int staffSize, bool graceSize) const
{
    const Resources &resources = this->GetResources();
    const Glyph *glyph = resources.GetGlyph(code);
    assert(glyph);
    int advX = glyph->GetHorizAdvX();
    advX = advX * m_drawingSmuflFontSize / glyph->GetUnitsPerEm();
    if (graceSize) advX = advX * m_options->m_graceFactor.GetValue();
    advX = advX * staffSize / 100;
    return advX;
}

Point Doc::ConvertFontPoint(const Glyph *glyph, const Point &fontPoint, int staffSize, bool graceSize) const
{
    assert(glyph);

    Point point;
    point.x = fontPoint.x * m_drawingSmuflFontSize / glyph->GetUnitsPerEm();
    point.y = fontPoint.y * m_drawingSmuflFontSize / glyph->GetUnitsPerEm();
    if (graceSize) {
        point.x = point.x * m_options->m_graceFactor.GetValue();
        point.y = point.y * m_options->m_graceFactor.GetValue();
    }
    if (staffSize != 100) {
        point.x = point.x * staffSize / 100;
        point.y = point.y * staffSize / 100;
    }
    return point;
}

int Doc::GetGlyphLeft(wchar_t code, int staffSize, bool graceSize) const
{
    int x, y, w, h;
    const Resources &resources = this->GetResources();
    const Glyph *glyph = resources.GetGlyph(code);
    assert(glyph);
    glyph->GetBoundingBox(x, y, w, h);
    x = x * m_drawingSmuflFontSize / glyph->GetUnitsPerEm();
    if (graceSize) x = x * m_options->m_graceFactor.GetValue();
    x = x * staffSize / 100;
    return x;
}

int Doc::GetGlyphRight(wchar_t code, int staffSize, bool graceSize) const
{
    return this->GetGlyphLeft(code, staffSize, graceSize) + this->GetGlyphWidth(code, staffSize, graceSize);
}

int Doc::GetGlyphBottom(wchar_t code, int staffSize, bool graceSize) const
{
    int x, y, w, h;
    const Resources &resources = this->GetResources();
    const Glyph *glyph = resources.GetGlyph(code);
    assert(glyph);
    glyph->GetBoundingBox(x, y, w, h);
    y = y * m_drawingSmuflFontSize / glyph->GetUnitsPerEm();
    if (graceSize) y = y * m_options->m_graceFactor.GetValue();
    y = y * staffSize / 100;
    return y;
}

int Doc::GetGlyphTop(wchar_t code, int staffSize, bool graceSize) const
{
    return this->GetGlyphBottom(code, staffSize, graceSize) + this->GetGlyphHeight(code, staffSize, graceSize);
}

int Doc::GetTextGlyphHeight(wchar_t code, FontInfo *font, bool graceSize) const
{
    assert(font);

    int x, y, w, h;
    const Resources &resources = this->GetResources();
    const Glyph *glyph = resources.GetTextGlyph(code);
    assert(glyph);
    glyph->GetBoundingBox(x, y, w, h);
    h = h * font->GetPointSize() / glyph->GetUnitsPerEm();
    if (graceSize) h = h * m_options->m_graceFactor.GetValue();
    return h;
}

int Doc::GetTextGlyphWidth(wchar_t code, FontInfo *font, bool graceSize) const
{
    assert(font);

    int x, y, w, h;
    const Resources &resources = this->GetResources();
    const Glyph *glyph = resources.GetTextGlyph(code);
    assert(glyph);
    glyph->GetBoundingBox(x, y, w, h);
    w = w * font->GetPointSize() / glyph->GetUnitsPerEm();
    if (graceSize) w = w * m_options->m_graceFactor.GetValue();
    return w;
}

int Doc::GetTextGlyphAdvX(wchar_t code, FontInfo *font, bool graceSize) const
{
    assert(font);

    const Resources &resources = this->GetResources();
    const Glyph *glyph = resources.GetTextGlyph(code);
    assert(glyph);
    int advX = glyph->GetHorizAdvX();
    advX = advX * font->GetPointSize() / glyph->GetUnitsPerEm();
    if (graceSize) advX = advX * m_options->m_graceFactor.GetValue();
    return advX;
}

int Doc::GetTextGlyphDescender(wchar_t code, FontInfo *font, bool graceSize) const
{
    assert(font);

    int x, y, w, h;
    const Resources &resources = this->GetResources();
    const Glyph *glyph = resources.GetTextGlyph(code);
    assert(glyph);
    glyph->GetBoundingBox(x, y, w, h);
    y = y * font->GetPointSize() / glyph->GetUnitsPerEm();
    if (graceSize) y = y * m_options->m_graceFactor.GetValue();
    return y;
}

int Doc::GetTextLineHeight(FontInfo *font, bool graceSize) const
{
    int descender = -this->GetTextGlyphDescender(L'q', font, graceSize);
    int height = this->GetTextGlyphHeight(L'I', font, graceSize);

    int lineHeight = ((descender + height) * 1.1);
    if (font->GetSupSubScript()) lineHeight /= SUPER_SCRIPT_FACTOR;

    return lineHeight;
}

int Doc::GetTextXHeight(FontInfo *font, bool graceSize) const
{
    return this->GetTextGlyphHeight('x', font, graceSize);
}

int Doc::GetDrawingUnit(int staffSize) const
{
    return m_options->m_unit.GetValue() * staffSize / 100;
}

int Doc::GetDrawingDoubleUnit(int staffSize) const
{
    return m_options->m_unit.GetValue() * 2 * staffSize / 100;
}

int Doc::GetDrawingStaffSize(int staffSize) const
{
    return m_options->m_unit.GetValue() * 8 * staffSize / 100;
}

int Doc::GetDrawingOctaveSize(int staffSize) const
{
    return m_options->m_unit.GetValue() * 7 * staffSize / 100;
}

int Doc::GetDrawingBrevisWidth(int staffSize) const
{
    return m_drawingBrevisWidth * staffSize / 100;
}

int Doc::GetDrawingBarLineWidth(int staffSize) const
{
    return m_options->m_barLineWidth.GetValue() * this->GetDrawingUnit(staffSize);
}

int Doc::GetDrawingStaffLineWidth(int staffSize) const
{
    return m_options->m_staffLineWidth.GetValue() * this->GetDrawingUnit(staffSize);
}

int Doc::GetDrawingStemWidth(int staffSize) const
{
    return m_options->m_stemWidth.GetValue() * this->GetDrawingUnit(staffSize);
}

int Doc::GetDrawingDynamHeight(int staffSize, bool withMargin) const
{
    int height = this->GetGlyphHeight(SMUFL_E522_dynamicForte, staffSize, false);
    // This should be styled
    if (withMargin) height += this->GetDrawingUnit(staffSize);
    return height;
}

int Doc::GetDrawingHairpinSize(int staffSize, bool withMargin) const
{
    int size = m_options->m_hairpinSize.GetValue() * this->GetDrawingUnit(staffSize);
    // This should be styled
    if (withMargin) size += this->GetDrawingUnit(staffSize);
    return size;
}

int Doc::GetDrawingBeamWidth(int staffSize, bool graceSize) const
{
    int value = m_drawingBeamWidth * staffSize / 100;
    if (graceSize) value = value * m_options->m_graceFactor.GetValue();
    return value;
}

int Doc::GetDrawingBeamWhiteWidth(int staffSize, bool graceSize) const
{
    int value = m_drawingBeamWhiteWidth * staffSize / 100;
    if (graceSize) value = value * m_options->m_graceFactor.GetValue();
    return value;
}

int Doc::GetDrawingLedgerLineExtension(int staffSize, bool graceSize) const
{
    int value = m_options->m_ledgerLineExtension.GetValue() * this->GetDrawingUnit(staffSize);
    if (graceSize) value = this->GetCueSize(value);
    return value;
}

int Doc::GetDrawingMinimalLedgerLineExtension(int staffSize, bool graceSize) const
{
    int value = m_options->m_ledgerLineExtension.GetMin() * this->GetDrawingUnit(staffSize);
    if (graceSize) value = this->GetCueSize(value);
    return value;
}

int Doc::GetCueSize(int value) const
{
    return value * this->GetCueScaling();
}

double Doc::GetCueScaling() const
{
    return m_options->m_graceFactor.GetValue();
}

FontInfo *Doc::GetDrawingSmuflFont(int staffSize, bool graceSize)
{
    m_drawingSmuflFont.SetFaceName(m_options->m_font.GetValue().c_str());
    int value = m_drawingSmuflFontSize * staffSize / 100;
    if (graceSize) value = value * m_options->m_graceFactor.GetValue();
    m_drawingSmuflFont.SetPointSize(value);
    return &m_drawingSmuflFont;
}

FontInfo *Doc::GetDrawingLyricFont(int staffSize)
{
    m_drawingLyricFont.SetPointSize(m_drawingLyricFontSize * staffSize / 100);
    return &m_drawingLyricFont;
}

FontInfo *Doc::GetFingeringFont(int staffSize)
{
    m_fingeringFont.SetPointSize(m_fingeringFontSize * staffSize / 100);
    return &m_fingeringFont;
}

double Doc::GetLeftMargin(const ClassId classId) const
{
    if (classId == ACCID) return m_options->m_leftMarginAccid.GetValue();
    if (classId == BARLINE) return m_options->m_leftMarginBarLine.GetValue();
    if (classId == BEATRPT) return m_options->m_leftMarginBeatRpt.GetValue();
    if (classId == CHORD) return m_options->m_leftMarginChord.GetValue();
    if (classId == CLEF) return m_options->m_leftMarginClef.GetValue();
    if (classId == KEYSIG) return m_options->m_leftMarginKeySig.GetValue();
    if (classId == MENSUR) return m_options->m_leftMarginMensur.GetValue();
    if (classId == METERSIG) return m_options->m_leftMarginMeterSig.GetValue();
    if (classId == MREST) return m_options->m_leftMarginMRest.GetValue();
    if (classId == MRPT2) return m_options->m_leftMarginMRpt2.GetValue();
    if (classId == MULTIREST) return m_options->m_leftMarginMultiRest.GetValue();
    if (classId == MULTIRPT) return m_options->m_leftMarginMultiRpt.GetValue();
    if (classId == NOTE) return m_options->m_leftMarginNote.GetValue();
    if (classId == STEM) return m_options->m_leftMarginNote.GetValue();
    if (classId == REST) return m_options->m_leftMarginRest.GetValue();
    if (classId == TABDURSYM) return m_options->m_leftMarginTabDurSym.GetValue();
    return m_options->m_defaultLeftMargin.GetValue();
}

double Doc::GetLeftMargin(Object *object) const
{
    assert(object);
    const ClassId id = object->GetClassId();
    if (id == BARLINE) {
        BarLine *barLine = vrv_cast<BarLine *>(object);
        switch (barLine->GetPosition()) {
            case BarLinePosition::None: return m_options->m_leftMarginBarLine.GetValue();
            case BarLinePosition::Left: return m_options->m_leftMarginLeftBarLine.GetValue();
            case BarLinePosition::Right: return m_options->m_leftMarginRightBarLine.GetValue();
            default: break;
        }
    }
    return this->GetLeftMargin(id);
}

double Doc::GetRightMargin(const ClassId classId) const
{
    if (classId == ACCID) return m_options->m_rightMarginAccid.GetValue();
    if (classId == BARLINE) return m_options->m_rightMarginBarLine.GetValue();
    if (classId == BEATRPT) return m_options->m_rightMarginBeatRpt.GetValue();
    if (classId == CHORD) return m_options->m_rightMarginChord.GetValue();
    if (classId == CLEF) return m_options->m_rightMarginClef.GetValue();
    if (classId == KEYSIG) return m_options->m_rightMarginKeySig.GetValue();
    if (classId == MENSUR) return m_options->m_rightMarginMensur.GetValue();
    if (classId == METERSIG) return m_options->m_rightMarginMeterSig.GetValue();
    if (classId == MREST) return m_options->m_rightMarginMRest.GetValue();
    if (classId == MRPT2) return m_options->m_rightMarginMRpt2.GetValue();
    if (classId == MULTIREST) return m_options->m_rightMarginMultiRest.GetValue();
    if (classId == MULTIRPT) return m_options->m_rightMarginMultiRpt.GetValue();
    if (classId == NOTE) return m_options->m_rightMarginNote.GetValue();
    if (classId == STEM) return m_options->m_rightMarginNote.GetValue();
    if (classId == REST) return m_options->m_rightMarginRest.GetValue();
    if (classId == TABDURSYM) return m_options->m_rightMarginTabDurSym.GetValue();
    return m_options->m_defaultRightMargin.GetValue();
}

double Doc::GetRightMargin(Object *object) const
{
    assert(object);
    const ClassId id = object->GetClassId();
    if (id == BARLINE) {
        BarLine *barLine = vrv_cast<BarLine *>(object);
        switch (barLine->GetPosition()) {
            case BarLinePosition::None: return m_options->m_rightMarginBarLine.GetValue();
            case BarLinePosition::Left: return m_options->m_rightMarginLeftBarLine.GetValue();
            case BarLinePosition::Right: return m_options->m_rightMarginRightBarLine.GetValue();
            default: break;
        }
    }
    return this->GetRightMargin(id);
}

double Doc::GetBottomMargin(const ClassId classId) const
{
    if (classId == ARTIC) return m_options->m_bottomMarginArtic.GetValue();
    if (classId == HARM) return m_options->m_bottomMarginHarm.GetValue();
    return m_options->m_defaultBottomMargin.GetValue();
}

double Doc::GetTopMargin(const ClassId classId) const
{
    if (classId == ARTIC) return m_options->m_topMarginArtic.GetValue();
    if (classId == HARM) return m_options->m_topMarginHarm.GetValue();
    return m_options->m_defaultTopMargin.GetValue();
}

double Doc::GetStaffDistance(const ClassId classId, int staffIndex, data_STAFFREL staffPosition)
{
    double distance = 0.0;
    if (staffPosition == STAFFREL_above || staffPosition == STAFFREL_below) {
        if (classId == DYNAM) {
            distance = m_options->m_dynamDist.GetDefault();

            // Inspect the scoreDef attribute
            if (this->GetCurrentScoreDef()->HasDynamDist()) {
                distance = this->GetCurrentScoreDef()->GetDynamDist();
            }

            // Inspect the staffDef attributes
            const StaffDef *staffDef = this->GetCurrentScoreDef()->GetStaffDef(staffIndex);
            if (staffDef != NULL && staffDef->HasDynamDist()) {
                distance = staffDef->GetDynamDist();
            }

            // Apply CLI option if set
            if (m_options->m_dynamDist.IsSet()) {
                distance = m_options->m_dynamDist.GetValue();
            }
        }
        else if (classId == HARM) {
            distance = m_options->m_harmDist.GetDefault();

            // Inspect the scoreDef attribute
            if (this->GetCurrentScoreDef()->HasHarmDist()) {
                distance = this->GetCurrentScoreDef()->GetHarmDist();
            }

            // Inspect the staffDef attributes
            const StaffDef *staffDef = this->GetCurrentScoreDef()->GetStaffDef(staffIndex);
            if (staffDef != NULL && staffDef->HasHarmDist()) {
                distance = staffDef->GetHarmDist();
            }

            // Apply CLI option if set
            if (m_options->m_harmDist.IsSet()) {
                distance = m_options->m_harmDist.GetValue();
            }
        }
    }
    return distance;
}

Page *Doc::SetDrawingPage(int pageIdx)
{
    // out of range
    if (!HasPage(pageIdx)) {
        return NULL;
    }
    // nothing to do
    if (m_drawingPage && m_drawingPage->GetIdx() == pageIdx) {
        return m_drawingPage;
    }
    Pages *pages = this->GetPages();
    assert(pages);
    m_drawingPage = vrv_cast<Page *>(pages->GetChild(pageIdx));
    assert(m_drawingPage);

    int glyph_size;

    // we use the page members only if set (!= -1)
    if (m_drawingPage->m_pageHeight != -1) {
        m_drawingPageHeight = m_drawingPage->m_pageHeight;
        m_drawingPageWidth = m_drawingPage->m_pageWidth;
        m_drawingPageMarginBottom = m_drawingPage->m_pageMarginBottom;
        m_drawingPageMarginLeft = m_drawingPage->m_pageMarginLeft;
        m_drawingPageMarginRight = m_drawingPage->m_pageMarginRight;
        m_drawingPageMarginTop = m_drawingPage->m_pageMarginTop;
    }
    else if (m_pageHeight != -1) {
        m_drawingPageHeight = m_pageHeight;
        m_drawingPageWidth = m_pageWidth;
        m_drawingPageMarginBottom = m_pageMarginBottom;
        m_drawingPageMarginLeft = m_pageMarginLeft;
        m_drawingPageMarginRight = m_pageMarginRight;
        m_drawingPageMarginTop = m_pageMarginTop;
    }
    else {
        m_drawingPageHeight = m_options->m_pageHeight.GetValue();
        m_drawingPageWidth = m_options->m_pageWidth.GetValue();
        m_drawingPageMarginBottom = m_options->m_pageMarginBottom.GetValue();
        m_drawingPageMarginLeft = m_options->m_pageMarginLeft.GetValue();
        m_drawingPageMarginRight = m_options->m_pageMarginRight.GetValue();
        m_drawingPageMarginTop = m_options->m_pageMarginTop.GetValue();
    }

    if (m_options->m_landscape.GetValue()) {
        int pageHeight = m_drawingPageWidth;
        m_drawingPageWidth = m_drawingPageHeight;
        m_drawingPageHeight = pageHeight;
        int pageMarginRight = m_drawingPageMarginLeft;
        m_drawingPageMarginLeft = m_drawingPageMarginRight;
        m_drawingPageMarginRight = pageMarginRight;
    }

    m_drawingPageContentHeight = m_drawingPageHeight - m_drawingPageMarginTop - m_drawingPageMarginBottom;
    m_drawingPageContentWidth = m_drawingPageWidth - m_drawingPageMarginLeft - m_drawingPageMarginRight;

    // From here we could check if values have changed
    // Since m_options->m_interlDefin stays the same, it's useless to do it
    // every time for now.

    m_drawingBeamMaxSlope = m_options->m_beamMaxSlope.GetValue();
    m_drawingBeamMinSlope = m_options->m_beamMinSlope.GetValue();
    m_drawingBeamMaxSlope /= 100;
    m_drawingBeamMinSlope /= 100;

    // values for beams
    m_drawingBeamWidth = m_options->m_unit.GetValue();
    m_drawingBeamWhiteWidth = m_options->m_unit.GetValue() / 2;

    // values for fonts
    m_drawingSmuflFontSize = CalcMusicFontSize();
    m_drawingLyricFontSize = m_options->m_unit.GetValue() * m_options->m_lyricSize.GetValue();
    m_fingeringFontSize = m_drawingLyricFontSize * m_options->m_fingeringScale.GetValue();

    glyph_size = this->GetGlyphWidth(SMUFL_E0A2_noteheadWhole, 100, 0);

    m_drawingBrevisWidth = (int)((glyph_size * 0.8) / 2);

    return m_drawingPage;
}

int Doc::CalcMusicFontSize()
{
    return m_options->m_unit.GetValue() * 8;
}

int Doc::GetAdjustedDrawingPageHeight() const
{
    assert(m_drawingPage);

    if ((this->GetType() == Transcription) || (this->GetType() == Facs)) {
        return m_drawingPage->m_pageHeight / DEFINITION_FACTOR;
    }

    int contentHeight = m_drawingPage->GetContentHeight();
    return (contentHeight + m_drawingPageMarginTop + m_drawingPageMarginBottom) / DEFINITION_FACTOR;
}

int Doc::GetAdjustedDrawingPageWidth() const
{
    assert(m_drawingPage);

    if ((this->GetType() == Transcription) || (this->GetType() == Facs)) {
        return m_drawingPage->m_pageWidth / DEFINITION_FACTOR;
    }

    int contentWidth = m_drawingPage->GetContentWidth();
    return (contentWidth + m_drawingPageMarginLeft + m_drawingPageMarginRight) / DEFINITION_FACTOR;
}

Score *Doc::GetCurrentScore()
{
    if (!m_currentScore) {
        m_currentScore = vrv_cast<Score *>(this->FindDescendantByType(SCORE));
        assert(m_currentScore);
    }
    return m_currentScore;
}

ScoreDef *Doc::GetCurrentScoreDef()
{
    if (!m_currentScore) this->GetCurrentScore();

    return m_currentScore->GetScoreDef();
}

void Doc::SetCurrentScore(Score *score)
{
    m_currentScore = score;
}

//----------------------------------------------------------------------------
// Doc functors methods
//----------------------------------------------------------------------------

int Doc::PrepareLyricsEnd(FunctorParams *functorParams)
{
    PrepareLyricsParams *params = vrv_params_cast<PrepareLyricsParams *>(functorParams);
    assert(params);
    if (!params->m_currentSyl) {
        return FUNCTOR_STOP; // early return
    }
    if (params->m_lastNoteOrChord && (params->m_currentSyl->GetStart() != params->m_lastNoteOrChord)) {
        params->m_currentSyl->SetEnd(params->m_lastNoteOrChord);
    }
    else if (m_options->m_openControlEvents.GetValue()) {
        sylLog_WORDPOS wordpos = params->m_currentSyl->GetWordpos();
        if ((wordpos == sylLog_WORDPOS_i) || (wordpos == sylLog_WORDPOS_m)) {
            Measure *lastMeasure = vrv_cast<Measure *>(this->FindDescendantByType(MEASURE, UNLIMITED_DEPTH, BACKWARD));
            assert(lastMeasure);
            params->m_currentSyl->SetEnd(lastMeasure->GetRightBarLine());
        }
    }

    return FUNCTOR_STOP;
}

int Doc::PrepareTimestampsEnd(FunctorParams *functorParams)
{
    PrepareTimestampsParams *params = vrv_params_cast<PrepareTimestampsParams *>(functorParams);
    assert(params);

    if (!m_options->m_openControlEvents.GetValue() || params->m_timeSpanningInterfaces.empty()) {
        return FUNCTOR_CONTINUE;
    }

    Measure *lastMeasure = dynamic_cast<Measure *>(this->FindDescendantByType(MEASURE, UNLIMITED_DEPTH, BACKWARD));
    if (!lastMeasure) {
        return FUNCTOR_CONTINUE;
    }

    for (auto &pair : params->m_timeSpanningInterfaces) {
        TimeSpanningInterface *interface = pair.first;
        assert(interface);
        if (!interface->GetEnd()) {
            interface->SetEnd(lastMeasure->GetRightBarLine());
        }
    }

    return FUNCTOR_CONTINUE;
}

} // namespace vrv
