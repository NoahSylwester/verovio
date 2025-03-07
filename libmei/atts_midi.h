/////////////////////////////////////////////////////////////////////////////
// Authors:     Laurent Pugin and Rodolfo Zitellini
// Created:     2014
// Copyright (c) Authors and others. All rights reserved.
//
// Code generated using a modified version of libmei
// by Andrew Hankinson, Alastair Porter, and Others
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// NOTE: this file was generated with the Verovio libmei version and
// should not be edited because changes will be lost.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_ATTS_MIDI_H__
#define __VRV_ATTS_MIDI_H__

#include "att.h"
#include "attdef.h"
#include "pugixml.hpp"

//----------------------------------------------------------------------------

#include <string>

namespace vrv {

//----------------------------------------------------------------------------
// AttChannelized
//----------------------------------------------------------------------------

class AttChannelized : public Att {
public:
    AttChannelized();
    virtual ~AttChannelized();

    /** Reset the default values for the attribute class **/
    void ResetChannelized();

    /** Read the values for the attribute class **/
    bool ReadChannelized(pugi::xml_node element);

    /** Write the values for the attribute class **/
    bool WriteChannelized(pugi::xml_node element);

    /**
     * @name Setters, getters and presence checker for class members.
     * The checker returns true if the attribute class is set (e.g., not equal
     * to the default value)
     **/
    ///@{
    void SetMidiChannel(data_MIDICHANNEL midiChannel_) { m_midiChannel = midiChannel_; }
    data_MIDICHANNEL GetMidiChannel() const { return m_midiChannel; }
    bool HasMidiChannel() const;
    //
    void SetMidiDuty(data_PERCENT_LIMITED midiDuty_) { m_midiDuty = midiDuty_; }
    data_PERCENT_LIMITED GetMidiDuty() const { return m_midiDuty; }
    bool HasMidiDuty() const;
    //
    void SetMidiPort(data_MIDIVALUE_NAME midiPort_) { m_midiPort = midiPort_; }
    data_MIDIVALUE_NAME GetMidiPort() const { return m_midiPort; }
    bool HasMidiPort() const;
    //
    void SetMidiTrack(int midiTrack_) { m_midiTrack = midiTrack_; }
    int GetMidiTrack() const { return m_midiTrack; }
    bool HasMidiTrack() const;
    ///@}

private:
    /** Records a MIDI channel value. **/
    data_MIDICHANNEL m_midiChannel;
    /** Specifies the 'on' part of the duty cycle as a percentage of a note's duration. **/
    data_PERCENT_LIMITED m_midiDuty;
    /** Sets the MIDI port value. **/
    data_MIDIVALUE_NAME m_midiPort;
    /** Sets the MIDI track. **/
    int m_midiTrack;

    /* include <attmidi.track> */
};

//----------------------------------------------------------------------------
// AttInstrumentIdent
//----------------------------------------------------------------------------

class AttInstrumentIdent : public Att {
public:
    AttInstrumentIdent();
    virtual ~AttInstrumentIdent();

    /** Reset the default values for the attribute class **/
    void ResetInstrumentIdent();

    /** Read the values for the attribute class **/
    bool ReadInstrumentIdent(pugi::xml_node element);

    /** Write the values for the attribute class **/
    bool WriteInstrumentIdent(pugi::xml_node element);

    /**
     * @name Setters, getters and presence checker for class members.
     * The checker returns true if the attribute class is set (e.g., not equal
     * to the default value)
     **/
    ///@{
    void SetInstr(std::string instr_) { m_instr = instr_; }
    std::string GetInstr() const { return m_instr; }
    bool HasInstr() const;
    ///@}

private:
    /**
     * Provides a way of pointing to a MIDI instrument definition.
     * It must contain the ID of an
     **/
    std::string m_instr;

    /* include <attinstr> */
};

//----------------------------------------------------------------------------
// AttMidiInstrument
//----------------------------------------------------------------------------

class AttMidiInstrument : public Att {
public:
    AttMidiInstrument();
    virtual ~AttMidiInstrument();

    /** Reset the default values for the attribute class **/
    void ResetMidiInstrument();

    /** Read the values for the attribute class **/
    bool ReadMidiInstrument(pugi::xml_node element);

    /** Write the values for the attribute class **/
    bool WriteMidiInstrument(pugi::xml_node element);

    /**
     * @name Setters, getters and presence checker for class members.
     * The checker returns true if the attribute class is set (e.g., not equal
     * to the default value)
     **/
    ///@{
    void SetMidiInstrnum(data_MIDIVALUE midiInstrnum_) { m_midiInstrnum = midiInstrnum_; }
    data_MIDIVALUE GetMidiInstrnum() const { return m_midiInstrnum; }
    bool HasMidiInstrnum() const;
    //
    void SetMidiInstrname(data_MIDINAMES midiInstrname_) { m_midiInstrname = midiInstrname_; }
    data_MIDINAMES GetMidiInstrname() const { return m_midiInstrname; }
    bool HasMidiInstrname() const;
    //
    void SetMidiPan(data_MIDIVALUE_PAN midiPan_) { m_midiPan = midiPan_; }
    data_MIDIVALUE_PAN GetMidiPan() const { return m_midiPan; }
    bool HasMidiPan() const;
    //
    void SetMidiPatchname(std::string midiPatchname_) { m_midiPatchname = midiPatchname_; }
    std::string GetMidiPatchname() const { return m_midiPatchname; }
    bool HasMidiPatchname() const;
    //
    void SetMidiPatchnum(data_MIDIVALUE midiPatchnum_) { m_midiPatchnum = midiPatchnum_; }
    data_MIDIVALUE GetMidiPatchnum() const { return m_midiPatchnum; }
    bool HasMidiPatchnum() const;
    //
    void SetMidiVolume(data_PERCENT midiVolume_) { m_midiVolume = midiVolume_; }
    data_PERCENT GetMidiVolume() const { return m_midiVolume; }
    bool HasMidiVolume() const;
    ///@}

private:
    /**
     * Captures the General MIDI instrument number.
     * Use an integer for a 0-based value. An integer preceded by "in" indicates a
     * 1-based value.
     **/
    data_MIDIVALUE m_midiInstrnum;
    /** Provides a General MIDI label for the MIDI instrument. **/
    data_MIDINAMES m_midiInstrname;
    /**
     * Sets the instrument's position in a stereo field.
     * MIDI values of 0 and 1 both pan left, 127 or 128 pans right, and 63 or 64 pans
     * to the center. Positve percentage values pan to the right, negative ones to the
     * left. 0% is centered.
     **/
    data_MIDIVALUE_PAN m_midiPan;
    /** Records a non-General MIDI patch/instrument name. **/
    std::string m_midiPatchname;
    /** Records a non-General MIDI patch/instrument number. **/
    data_MIDIVALUE m_midiPatchnum;
    /** Sets the instrument's volume. **/
    data_PERCENT m_midiVolume;

    /* include <attmidi.volume> */
};

//----------------------------------------------------------------------------
// AttMidiNumber
//----------------------------------------------------------------------------

class AttMidiNumber : public Att {
public:
    AttMidiNumber();
    virtual ~AttMidiNumber();

    /** Reset the default values for the attribute class **/
    void ResetMidiNumber();

    /** Read the values for the attribute class **/
    bool ReadMidiNumber(pugi::xml_node element);

    /** Write the values for the attribute class **/
    bool WriteMidiNumber(pugi::xml_node element);

    /**
     * @name Setters, getters and presence checker for class members.
     * The checker returns true if the attribute class is set (e.g., not equal
     * to the default value)
     **/
    ///@{
    void SetNum(int num_) { m_num = num_; }
    int GetNum() const { return m_num; }
    bool HasNum() const;
    ///@}

private:
    /** Records a number or count accompanying a notational feature. **/
    int m_num;

    /* include <attnum> */
};

//----------------------------------------------------------------------------
// AttMidiTempo
//----------------------------------------------------------------------------

class AttMidiTempo : public Att {
public:
    AttMidiTempo();
    virtual ~AttMidiTempo();

    /** Reset the default values for the attribute class **/
    void ResetMidiTempo();

    /** Read the values for the attribute class **/
    bool ReadMidiTempo(pugi::xml_node element);

    /** Write the values for the attribute class **/
    bool WriteMidiTempo(pugi::xml_node element);

    /**
     * @name Setters, getters and presence checker for class members.
     * The checker returns true if the attribute class is set (e.g., not equal
     * to the default value)
     **/
    ///@{
    void SetMidiBpm(double midiBpm_) { m_midiBpm = midiBpm_; }
    double GetMidiBpm() const { return m_midiBpm; }
    bool HasMidiBpm() const;
    //
    void SetMidiMspb(data_MIDIMSPB midiMspb_) { m_midiMspb = midiMspb_; }
    data_MIDIMSPB GetMidiMspb() const { return m_midiMspb; }
    bool HasMidiMspb() const;
    ///@}

private:
    /**
     * Captures the number of *quarter notes* per minute.
     * In MIDI, a beat is always defined as a quarter note, *not the numerator of the
     * time signature or the metronomic indication*.
     **/
    double m_midiBpm;
    /**
     * Records the number of microseconds per *quarter note*.
     * In MIDI, a beat is always defined as a quarter note, *not the numerator of the
     * time signature or the metronomic indication*. At 120 quarter notes per minute,
     * each quarter note will last 500,000 microseconds.
     **/
    data_MIDIMSPB m_midiMspb;

    /* include <attmidi.mspb> */
};

//----------------------------------------------------------------------------
// AttMidiValue
//----------------------------------------------------------------------------

class AttMidiValue : public Att {
public:
    AttMidiValue();
    virtual ~AttMidiValue();

    /** Reset the default values for the attribute class **/
    void ResetMidiValue();

    /** Read the values for the attribute class **/
    bool ReadMidiValue(pugi::xml_node element);

    /** Write the values for the attribute class **/
    bool WriteMidiValue(pugi::xml_node element);

    /**
     * @name Setters, getters and presence checker for class members.
     * The checker returns true if the attribute class is set (e.g., not equal
     * to the default value)
     **/
    ///@{
    void SetVal(data_MIDIVALUE val_) { m_val = val_; }
    data_MIDIVALUE GetVal() const { return m_val; }
    bool HasVal() const;
    ///@}

private:
    /** MIDI number. **/
    data_MIDIVALUE m_val;

    /* include <attval> */
};

//----------------------------------------------------------------------------
// AttMidiValue2
//----------------------------------------------------------------------------

class AttMidiValue2 : public Att {
public:
    AttMidiValue2();
    virtual ~AttMidiValue2();

    /** Reset the default values for the attribute class **/
    void ResetMidiValue2();

    /** Read the values for the attribute class **/
    bool ReadMidiValue2(pugi::xml_node element);

    /** Write the values for the attribute class **/
    bool WriteMidiValue2(pugi::xml_node element);

    /**
     * @name Setters, getters and presence checker for class members.
     * The checker returns true if the attribute class is set (e.g., not equal
     * to the default value)
     **/
    ///@{
    void SetVal2(data_MIDIVALUE val2_) { m_val2 = val2_; }
    data_MIDIVALUE GetVal2() const { return m_val2; }
    bool HasVal2() const;
    ///@}

private:
    /** MIDI number. **/
    data_MIDIVALUE m_val2;

    /* include <attval2> */
};

//----------------------------------------------------------------------------
// AttMidiVelocity
//----------------------------------------------------------------------------

class AttMidiVelocity : public Att {
public:
    AttMidiVelocity();
    virtual ~AttMidiVelocity();

    /** Reset the default values for the attribute class **/
    void ResetMidiVelocity();

    /** Read the values for the attribute class **/
    bool ReadMidiVelocity(pugi::xml_node element);

    /** Write the values for the attribute class **/
    bool WriteMidiVelocity(pugi::xml_node element);

    /**
     * @name Setters, getters and presence checker for class members.
     * The checker returns true if the attribute class is set (e.g., not equal
     * to the default value)
     **/
    ///@{
    void SetVel(data_MIDIVALUE vel_) { m_vel = vel_; }
    data_MIDIVALUE GetVel() const { return m_vel; }
    bool HasVel() const;
    ///@}

private:
    /** MIDI Note-on/off velocity. **/
    data_MIDIVALUE m_vel;

    /* include <attvel> */
};

//----------------------------------------------------------------------------
// AttTimeBase
//----------------------------------------------------------------------------

class AttTimeBase : public Att {
public:
    AttTimeBase();
    virtual ~AttTimeBase();

    /** Reset the default values for the attribute class **/
    void ResetTimeBase();

    /** Read the values for the attribute class **/
    bool ReadTimeBase(pugi::xml_node element);

    /** Write the values for the attribute class **/
    bool WriteTimeBase(pugi::xml_node element);

    /**
     * @name Setters, getters and presence checker for class members.
     * The checker returns true if the attribute class is set (e.g., not equal
     * to the default value)
     **/
    ///@{
    void SetPpq(int ppq_) { m_ppq = ppq_; }
    int GetPpq() const { return m_ppq; }
    bool HasPpq() const;
    ///@}

private:
    /**
     * Indicates the number of pulses (sometimes referred to as ticks or divisions) per
     * quarter note.
     * Unlike MIDI, MEI permits different values for a score and individual staves.
     **/
    int m_ppq;

    /* include <attppq> */
};

} // vrv namespace

#endif // __VRV_ATTS_MIDI_H__
