/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright 2000-2015 the Rosegarden development team.
 
    Other copyrights also apply to some parts of this work.  Please
    see the AUTHORS file and individual file headers for details.
 
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/


#include "TransposeCommand.h"

#include <iostream>
#include "base/NotationTypes.h"
#include "base/Selection.h"
#include "document/BasicSelectionCommand.h"
#include <QString>
#include "base/BaseProperties.h"


namespace Rosegarden
{

using namespace BaseProperties;
using namespace Accidentals;

void
TransposeCommand::modifySegment()
{
    EventSelection::eventcontainer::iterator i;
    
    for (i = m_selection->getSegmentEvents().begin();
         i != m_selection->getSegmentEvents().end(); ++i) {
        
        if ((*i)->isa(Note::EventType)) {
            
            if (m_diatonic) { 
            
                Pitch oldPitch(**i);
        
                timeT noteTime = (*i)->getAbsoluteTime();
                Key key = m_selection->getSegment().getKeyAtTime(noteTime);
                Pitch newPitch = oldPitch.transpose(key, m_semitones, m_steps);
                Event * newNoteEvent = newPitch.getAsNoteEvent(0, 0);
                Accidental newAccidental;
                newNoteEvent->get<String>(BaseProperties::ACCIDENTAL, newAccidental);

                (*i)->set<Int>(PITCH, newPitch.getPerformancePitch());
                (*i)->set<String>(ACCIDENTAL, newAccidental);
            } else {
                try {
                    long pitch = (*i)->get<Int>(PITCH);
                    pitch += m_semitones;
                    (*i)->set<Int>(PITCH, pitch);
                    if ((m_semitones % 12) != 0) {
                        (*i)->unset(ACCIDENTAL);
                    }
                } catch (...) { }
            }

        }
    }
}

}
