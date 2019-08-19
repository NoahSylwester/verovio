/////////////////////////////////////////////////////////////////////////////
// Name:        drawinginterface.cpp
// Author:      Laurent Pugin
// Created:     2015
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "drawinginterface.h"

//----------------------------------------------------------------------------

#include <assert.h>

//----------------------------------------------------------------------------

#include "chord.h"
#include "elementpart.h"
#include "layerelement.h"
#include "note.h"
#include "object.h"

namespace vrv {

//----------------------------------------------------------------------------
// DrawingListInterface
//----------------------------------------------------------------------------

DrawingListInterface::DrawingListInterface()
{
    Reset();
}

DrawingListInterface::~DrawingListInterface() {}

void DrawingListInterface::Reset()
{
    m_drawingList.clear();
}

void DrawingListInterface::AddToDrawingList(Object *object)
{
    if (std::find(m_drawingList.begin(), m_drawingList.end(), object) == m_drawingList.end()) {
        // someName not in name, add it
        m_drawingList.push_back(object);
    }

    /*
    m_drawingList.push_back(object);
    m_drawingList.sort();
    m_drawingList.unique();
     */
}

ArrayOfObjects *DrawingListInterface::GetDrawingList()
{
    return &m_drawingList;
}

void DrawingListInterface::ResetDrawingList()
{
    m_drawingList.clear();
}

//----------------------------------------------------------------------------
// BeamDrawingInterface
//----------------------------------------------------------------------------

BeamDrawingInterface::BeamDrawingInterface()
{
    Reset();
}

BeamDrawingInterface::~BeamDrawingInterface() {}

void BeamDrawingInterface::Reset()
{
    m_changingDur = false;
    m_beamHasChord = false;
    m_hasMultipleStemDir = false;
    m_cueSize = false;
    m_crossStaff = false;
    m_shortestDur = 0;
    m_stemDir = STEMDIRECTION_NONE;
    m_beamPlace = BEAMPLACE_NONE;
    
    m_beamWidth = 0;
    m_beamWidthBlack = 0;
    m_beamWidthWhite = 0;
}
    
void BeamDrawingInterface::ClearCoords()
{
    ArrayOfBeamElementCoords::iterator iter;
    for (iter = m_beamElementCoords.begin(); iter != m_beamElementCoords.end(); ++iter) {
        delete *iter;
    }
    m_beamElementCoords.clear();
}
    
    
void BeamDrawingInterface::InitCoords(ArrayOfObjects *childList)
{
    ClearCoords();

    if (childList->empty()) {
        return;
    }

    // duration variables
    int lastDur, currentDur;

    m_beamElementCoords.reserve(childList->size());
    int i;
    for (i = 0; i < (int)childList->size(); ++i) {
        m_beamElementCoords.push_back(new BeamElementCoord());
    }

    // current point to the first Note in the layed out layer
    LayerElement *current = dynamic_cast<LayerElement *>(childList->front());
    // Beam list should contain only DurationInterface objects
    assert(current->GetDurationInterface());

    lastDur = (current->GetDurationInterface())->GetActualDur();

    /******************************************************************/
    // Populate BeamElementCoord for each element in the beam
    // This could be moved to Beam::InitCoord for optimization because there should be no
    // need for redoing it everytime it is drawn.

    data_STEMDIRECTION currentStemDir;
    Layer *layer = NULL;
    Staff *currentStaff = NULL;

    int elementCount = 0;

    ArrayOfObjects::iterator iter = childList->begin();
    do {
        // Beam list should contain only DurationInterface objects
        assert(current->GetDurationInterface());
        currentDur = (current->GetDurationInterface())->GetActualDur();

        if (current->Is(CHORD)) {
            this->m_beamHasChord = true;
        }

        m_beamElementCoords.at(elementCount)->m_element = current;
        m_beamElementCoords.at(elementCount)->m_dur = currentDur;

        // Look at beam breaks
        m_beamElementCoords.at(elementCount)->m_breaksec = 0;
        AttBeamSecondary *beamsecondary = dynamic_cast<AttBeamSecondary *>(current);
        if (beamsecondary && beamsecondary->HasBreaksec()) {
            if (!this->m_changingDur) this->m_changingDur = true;
            m_beamElementCoords.at(elementCount)->m_breaksec = beamsecondary->GetBreaksec();
        }

        Staff *staff = current->GetCrossStaff(layer);
        if (staff != currentStaff) {
            this->m_crossStaff = true;
        }
        currentStaff = staff;

        // Skip rests
        if (current->Is({ NOTE, CHORD })) {
            // look at the stemDir to see if we have multiple stem Dir
            if (!this->m_hasMultipleStemDir) {
                StemmedDrawingInterface *interface = current->GetStemmedDrawingInterface();
                assert(interface);
                Stem *stem = interface->GetDrawingStem();
                // This can be NULL but should not
                m_beamElementCoords.at(elementCount)->m_stem = stem;
                currentStemDir = STEMDIRECTION_NONE;
                if (stem) {
                    assert(dynamic_cast<AttStems *>(stem));
                    currentStemDir = (dynamic_cast<AttStems *>(stem))->GetStemDir();
                }
                if (currentStemDir != STEMDIRECTION_NONE) {
                    if ((this->m_stemDir != STEMDIRECTION_NONE)
                        && (this->m_stemDir != currentStemDir)) {
                        this->m_hasMultipleStemDir = true;
                    }
                    this->m_stemDir = currentStemDir;
                }
            }
            // keep the shortest dur in the beam
            this->m_shortestDur = std::max(currentDur, this->m_shortestDur);
        }
        // check if we have more than duration in the beam
        if (!this->m_changingDur && currentDur != lastDur) this->m_changingDur = true;
        lastDur = currentDur;

        elementCount++;

        ++iter;
        if (iter == childList->end()) {
            break;
        }
        current = dynamic_cast<LayerElement *>(*iter);
        if (current == NULL) {
            LogDebug("Error accessing element in Beam list");
            return;
        }

    } while (1);

    // elementCount must be greater than 0 here
    if (elementCount == 0) {
        LogDebug("Beam with no notes of duration > 8 detected. Exiting DrawBeam.");
        return;
    }

    int last = elementCount - 1;

    // We look only at the last note for checking if cue-sized. Somehow arbitrarily
    this->m_cueSize = m_beamElementCoords.at(last)->m_element->GetDrawingCueSize();

    // Always set stem diretion to up for grace note beam unless stem direction is provided
    if (this->m_cueSize && (this->m_stemDir == STEMDIRECTION_NONE)) {
        this->m_stemDir = STEMDIRECTION_up;
    }
}

//----------------------------------------------------------------------------
// StaffDefDrawingInterface
//----------------------------------------------------------------------------

StaffDefDrawingInterface::StaffDefDrawingInterface()
{
    Reset();
}

StaffDefDrawingInterface::~StaffDefDrawingInterface() {}

void StaffDefDrawingInterface::Reset()
{
    m_currentClef.Reset();
    m_currentKeySig.Reset();
    m_currentMensur.Reset();
    m_currentMeterSig.Reset();

    if (m_currentClef.GetLine() > 4) {
        Clef clef2;
    }

    m_drawClef = false;
    m_drawKeySig = false;
    m_drawMensur = false;
    m_drawMeterSig = false;
}

void StaffDefDrawingInterface::SetCurrentClef(Clef *clef)
{
    if (clef) {
        m_currentClef = *clef;
    }
}

void StaffDefDrawingInterface::SetCurrentKeySig(KeySig *keySig)
{
    if (keySig) {
        char drawingCancelAccidCount = m_currentKeySig.GetAlterationNumber();
        data_ACCIDENTAL_WRITTEN drawingCancelAccidType = m_currentKeySig.GetAlterationType();
        m_currentKeySig = *keySig;
        m_currentKeySig.m_drawingCancelAccidCount = drawingCancelAccidCount;
        m_currentKeySig.m_drawingCancelAccidType = drawingCancelAccidType;
    }
}

void StaffDefDrawingInterface::SetCurrentMensur(Mensur *mensur)
{
    if (mensur) {
        m_currentMensur = *mensur;
    }
}

void StaffDefDrawingInterface::SetCurrentMeterSig(MeterSig *meterSig)
{
    if (meterSig) {
        m_currentMeterSig = *meterSig;
    }
}

//----------------------------------------------------------------------------
// StemmedDrawingInterface
//----------------------------------------------------------------------------

StemmedDrawingInterface::StemmedDrawingInterface()
{
    Reset();
}

StemmedDrawingInterface::~StemmedDrawingInterface() {}

void StemmedDrawingInterface::Reset()
{
    m_drawingStem = NULL;
}

void StemmedDrawingInterface::SetDrawingStem(Stem *stem)
{
    m_drawingStem = stem;
}

void StemmedDrawingInterface::SetDrawingStemDir(data_STEMDIRECTION stemDir)
{
    if (m_drawingStem) m_drawingStem->SetDrawingStemDir(stemDir);
}

data_STEMDIRECTION StemmedDrawingInterface::GetDrawingStemDir()
{
    if (m_drawingStem) return m_drawingStem->GetDrawingStemDir();
    return STEMDIRECTION_NONE;
}

void StemmedDrawingInterface::SetDrawingStemLen(int stemLen)
{
    if (m_drawingStem) m_drawingStem->SetDrawingStemLen(stemLen);
}

int StemmedDrawingInterface::GetDrawingStemLen()
{
    if (m_drawingStem) return m_drawingStem->GetDrawingStemLen();
    return 0;
}

Point StemmedDrawingInterface::GetDrawingStemStart(Object *object)
{
    assert(m_drawingStem || object);
    if (object && !m_drawingStem) {
        assert(this == dynamic_cast<StemmedDrawingInterface *>(object));
        return Point(object->GetDrawingX(), object->GetDrawingY());
    }
    return Point(m_drawingStem->GetDrawingX(), m_drawingStem->GetDrawingY());
}

Point StemmedDrawingInterface::GetDrawingStemEnd(Object *object)
{
    assert(m_drawingStem || object);
    if (object && !m_drawingStem) {
        assert(this == dynamic_cast<StemmedDrawingInterface *>(object));
        if (!m_drawingStem) {
            // Somehow arbitrary for chord - stem end it the bottom with no stem
            if (object->Is(CHORD)) {
                Chord *chord = dynamic_cast<Chord *>(object);
                assert(chord);
                return Point(object->GetDrawingX(), chord->GetYBottom());
            }
            return Point(object->GetDrawingX(), object->GetDrawingY());
        }
    }
    return Point(m_drawingStem->GetDrawingX(), m_drawingStem->GetDrawingY() - GetDrawingStemLen());
}

} // namespace vrv
