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

#define RG_MODULE_STRING "[RosegardenMainViewWidget]"

#include "RosegardenMainViewWidget.h"

#include "sound/Midi.h"
#include "sound/SoundDriver.h"
#include "gui/editors/segment/TrackButtons.h"
#include "misc/Debug.h"
#include "misc/Strings.h"
#include "misc/ConfigGroups.h"
#include "gui/application/TransportStatus.h"
#include "base/AudioLevel.h"
#include "base/Composition.h"
#include "base/Instrument.h"
#include "base/MidiDevice.h"
#include "base/MidiProgram.h"
#include "base/NotationTypes.h"
#include "base/RealTime.h"
#include "base/RulerScale.h"
#include "base/Segment.h"
#include "base/Selection.h"
#include "base/Studio.h"
#include "base/Track.h"
#include "commands/segment/AudioSegmentAutoSplitCommand.h"
#include "commands/segment/AudioSegmentInsertCommand.h"
#include "commands/segment/SegmentSingleRepeatToCopyCommand.h"
#include "document/CommandHistory.h"
#include "document/RosegardenDocument.h"
#include "RosegardenApplication.h"
#include "gui/configuration/GeneralConfigurationPage.h"
#include "gui/configuration/AudioConfigurationPage.h"
#include "gui/dialogs/AudioSplitDialog.h"
#include "gui/dialogs/AudioManagerDialog.h"
#include "gui/dialogs/DocumentConfigureDialog.h"
#include "gui/dialogs/TempoDialog.h"
#include "gui/editors/eventlist/EventView.h"
#include "gui/editors/matrix/MatrixView.h"
#include "gui/editors/notation/NotationView.h"
#include "gui/editors/parameters/InstrumentParameterBox.h"
#include "gui/editors/parameters/SegmentParameterBox.h"
#include "gui/editors/parameters/TrackParameterBox.h"
#include "gui/editors/pitchtracker/PitchTrackerView.h"
#include "gui/editors/segment/compositionview/CompositionView.h"
#include "gui/editors/segment/compositionview/SegmentSelector.h"
#include "gui/editors/segment/TrackEditor.h"
#include "gui/seqmanager/SequenceManager.h"
#include "gui/rulers/ChordNameRuler.h"
#include "gui/rulers/LoopRuler.h"
#include "gui/rulers/TempoRuler.h"
#include "gui/rulers/StandardRuler.h"
#include "gui/widgets/ProgressDialog.h"
#include "gui/widgets/CurrentProgressDialog.h"
#include "RosegardenMainWindow.h"
#include "SetWaitCursor.h"
#include "sound/AudioFile.h"
#include "sound/AudioFileManager.h"
#include "sound/MappedEvent.h"
#include "sound/SequencerDataBlock.h"
#include "document/Command.h"

#include <QApplication>
#include <QSettings>
#include <QMessageBox>
#include <QProcess>
#include <QApplication>
#include <QCursor>
#include <QDialog>
#include <QFileInfo>
#include <QObject>
#include <QString>
#include <QWidget>
#include <QVBoxLayout>
#include <QTextStream>
#include <QScrollBar>

#include "gui/editors/parameters/MIDIInstrumentParameterPanel.h"

namespace Rosegarden
{

// Use this to define the basic unit of the main segment canvas size.
//
// This apparently arbitrary figure is what we think is an
// appropriate width in pixels for a 4/4 bar.  Beware of making it
// too narrow, as shorter bars will be proportionally smaller --
// the visual difference between 2/4 and 4/4 is perhaps greater
// than it sounds.
//
static double barWidth44 = 100.0;

const QWidget *RosegardenMainViewWidget::m_lastActiveMainWindow = 0;

// This is the maximum number of matrix, event view or percussion
// matrix editors to open in a single operation (not the maximum that
// can be open at a time -- there isn't one)
//
static int maxEditorsToOpen = 8;

RosegardenMainViewWidget::RosegardenMainViewWidget(bool showTrackLabels,
                                     SegmentParameterBox* segmentParameterBox,
                                     InstrumentParameterBox* instrumentParameterBox,
                                     TrackParameterBox* trackParameterBox,
                                     QWidget *parent,
                                     const char* /*name*/)
        : QWidget(parent),
        m_rulerScale(0),
        m_trackEditor(0),
        m_segmentParameterBox(segmentParameterBox),
        m_instrumentParameterBox(instrumentParameterBox),
        m_trackParameterBox(trackParameterBox)
{
    setObjectName("View");
    RosegardenDocument* doc = getDocument();
    Composition *comp = &doc->getComposition();
    QVBoxLayout *layout = new QVBoxLayout;

    double unitsPerPixel =
        TimeSignature(4, 4).getBarDuration() / barWidth44;
    m_rulerScale = new SimpleRulerScale(comp, 0, unitsPerPixel);

    // Construct the trackEditor first so we can then
    // query it for placement information
    //
    m_trackEditor = new TrackEditor(doc, this, m_rulerScale, showTrackLabels);

    layout->addWidget(m_trackEditor);
    setLayout(layout);

    connect(m_trackEditor->getCompositionView(),
            SIGNAL(editSegment(Segment*)),
            SLOT(slotEditSegment(Segment*)));

    //connect(m_trackEditor->getCompositionView(),
    //        SIGNAL(editSegmentNotation(Segment*)),
    //        SLOT(slotEditSegmentNotation(Segment*)));

    //connect(m_trackEditor->getCompositionView(),
    //        SIGNAL(editSegmentPitchView(Segment*)),
    //        SLOT(slotEditSegmentPitchView(Segment*)));

    //connect(m_trackEditor->getCompositionView(),
    //        SIGNAL(editSegmentMatrix(Segment*)),
    //        SLOT(slotEditSegmentMatrix(Segment*)));

    //connect(m_trackEditor->getCompositionView(),
    //        SIGNAL(editSegmentAudio(Segment*)),
    //        SLOT(slotEditSegmentAudio(Segment*)));

    //connect(m_trackEditor->getCompositionView(),
    //        SIGNAL(audioSegmentAutoSplit(Segment*)),
    //        SLOT(slotSegmentAutoSplit(Segment*)));

    //connect(m_trackEditor->getCompositionView(),
    //        SIGNAL(editSegmentEventList(Segment*)),
    //        SLOT(slotEditSegmentEventList(Segment*)));

    connect(m_trackEditor->getCompositionView(),
            SIGNAL(editRepeat(Segment*, timeT)),
            SLOT(slotEditRepeat(Segment*, timeT)));

    connect(m_trackEditor->getCompositionView(),
            SIGNAL(setPointerPosition(timeT)),
            doc, SLOT(slotSetPointerPosition(timeT)));

    connect(m_trackEditor,
            SIGNAL(droppedDocument(QString)),
            parent,
            SLOT(slotOpenDroppedURL(QString)));

    connect(m_trackEditor,
            SIGNAL(droppedAudio(QString)),
            this,
            SLOT(slotDroppedAudio(QString)));

    connect(m_trackEditor,
            SIGNAL(droppedNewAudio(QString)),
            this,
            SLOT(slotDroppedNewAudio(QString)));

    connect(m_trackEditor->getTrackButtons(),
            SIGNAL(instrumentSelected(int)),
            m_trackParameterBox,
            SLOT(slotUpdateControls(int)));

    connect(m_trackParameterBox,
            SIGNAL(instrumentSelected(TrackId, int)),
            m_trackEditor->getTrackButtons(),
            SLOT(slotTPBInstrumentSelected(TrackId, int)));

    connect(this, SIGNAL(controllerDeviceEventReceived(MappedEvent *, const void *)),
            this, SLOT(slotControllerDeviceEventReceived(MappedEvent *, const void *)));

    if (doc) {
        /* signal no longer exists
        	connect(doc, SIGNAL(recordingSegmentUpdated(Segment *,
        						    timeT)),
        		this, SLOT(slotUpdateRecordingSegment(Segment *,
        						      timeT)));
        */

        QObject::connect
        (CommandHistory::getInstance(), SIGNAL(commandExecuted()),
         m_trackEditor->getCompositionView(), SLOT(slotUpdateAll()));
    }
}

RosegardenMainViewWidget::~RosegardenMainViewWidget()
{
    RG_DEBUG << "~RosegardenMainViewWidget()" << endl;
    delete m_rulerScale;
}

RosegardenDocument*
RosegardenMainViewWidget::getDocument() const
{
    return RosegardenMainWindow::self()->getDocument();
}

void RosegardenMainViewWidget::selectTool(QString toolName)
{
    m_trackEditor->getCompositionView()->setTool(toolName);
}

bool
RosegardenMainViewWidget::haveSelection()
{
    return m_trackEditor->getCompositionView()->haveSelection();
}

SegmentSelection
RosegardenMainViewWidget::getSelection()
{
    return m_trackEditor->getCompositionView()->getSelectedSegments();
}

void RosegardenMainViewWidget::updateSelectedSegments()
{
    m_trackEditor->getCompositionView()->updateSelectedSegments();
}

/* hjj: WHAT DO DO WITH THIS ?
void
RosegardenMainViewWidget::slotEditMetadata(QString name)
{
    const QWidget *ww = dynamic_cast<const QWidget *>(sender());
    QWidget *w = const_cast<QWidget *>(ww);

    DocumentConfigureDialog *configDlg =
        new DocumentConfigureDialog(getDocument(), w ? w : this);

    configDlg->selectMetadata(name);

    configDlg->show();
}
*/

void RosegardenMainViewWidget::slotEditSegment(Segment* segment)
{
    Segment::SegmentType type = Segment::Internal;

    if (segment) {
        type = segment->getType();
    } else {
        if (haveSelection()) {

            bool haveType = false;

            SegmentSelection selection = getSelection();
            for (SegmentSelection::iterator i = selection.begin();
                    i != selection.end(); ++i) {

                Segment::SegmentType myType = (*i)->getType();
                if (haveType) {
                    if (myType != type) {
                         QMessageBox::warning(this, tr("Rosegarden"), tr("Selection must contain only audio or non-audio segments"));
                        return ;
                    }
                } else {
                    type = myType;
                    haveType = true;
                    segment = *i;
                }
            }
        } else
            return ;
    }

    if (type == Segment::Audio) {
        slotEditSegmentAudio(segment);
    } else {

        QSettings settings;
        settings.beginGroup( GeneralOptionsConfigGroup );

        GeneralConfigurationPage::DoubleClickClient
        client =
            (GeneralConfigurationPage::DoubleClickClient)
            ( settings.value("doubleclickclient",
                                          (unsigned int) GeneralConfigurationPage::NotationView).toUInt());

        if (client == GeneralConfigurationPage::MatrixView) {

            bool isPercussion = false;
            Track *track = getDocument()->getComposition().getTrackById
                (segment->getTrack());
            if (track) {
                InstrumentId iid = track->getInstrument();
                Instrument *instrument =
                    getDocument()->getStudio().getInstrumentById(iid);
                if (instrument && instrument->isPercussion()) isPercussion = true;
            }

            if (isPercussion) {
                slotEditSegmentPercussionMatrix(segment);
            } else {
                slotEditSegmentMatrix(segment);
            }

        } else if (client == GeneralConfigurationPage::EventView) {
            slotEditSegmentEventList(segment);
        } else {
            slotEditSegmentNotation(segment);
        }
        settings.endGroup();
    }
}

void RosegardenMainViewWidget::slotEditRepeat(Segment *segment,
                                       timeT time)
{
    SegmentSingleRepeatToCopyCommand *command =
        new SegmentSingleRepeatToCopyCommand(segment, time);
    slotAddCommandToHistory(command);
}

void RosegardenMainViewWidget::slotEditSegmentNotation(Segment* p)
{
    SetWaitCursor waitCursor;
    std::vector<Segment *> segmentsToEdit;

    RG_DEBUG << "\n\n\n\nRosegardenMainViewWidget::slotEditSegmentNotation: p is " << p << endl;

    // The logic here is: If we're calling for this operation to
    // happen on a particular segment, then open that segment and if
    // it's part of a selection open all other selected segments too.
    // If we're not calling for any particular segment, then open all
    // selected segments if there are any.

    if (haveSelection()) {

        SegmentSelection selection = getSelection();

        if (!p || (selection.find(p) != selection.end())) {
            for (SegmentSelection::iterator i = selection.begin();
                    i != selection.end(); ++i) {
                if ((*i)->getType() != Segment::Audio) {
                    segmentsToEdit.push_back(*i);
                }
            }
        } else {
            if (p->getType() != Segment::Audio) {
                segmentsToEdit.push_back(p);
            }
        }

    } else if (p) {
        if (p->getType() != Segment::Audio) {
            segmentsToEdit.push_back(p);
        }
    } else {
        return ;
    }

    if (segmentsToEdit.empty()) {
         QMessageBox::warning(this, tr("Rosegarden"), tr("No non-audio segments selected"));
        return ;
    }

    slotEditSegmentsNotation(segmentsToEdit);
}

void RosegardenMainViewWidget::slotEditSegmentsNotation(std::vector<Segment *> segmentsToEdit)
{
    NotationView *view = createNotationView(segmentsToEdit);
    if (view) view->show();
}

NotationView *
RosegardenMainViewWidget::createNotationView(std::vector<Segment *> segmentsToEdit)
{
    NotationView *notationView =
        new NotationView(getDocument(), segmentsToEdit, this);

    // For tempo changes (ugh -- it'd be nicer to make a tempo change
    // command that could interpret all this stuff from the dialog)
    //
    connect(notationView, SIGNAL(changeTempo(timeT,
                                 tempoT,
                                 tempoT,
                                 TempoDialog::TempoDialogAction)),
            RosegardenMainWindow::self(), SLOT(slotChangeTempo(timeT,
                                           tempoT,
                                           tempoT,
                                           TempoDialog::TempoDialogAction)));


    connect(notationView, SIGNAL(windowActivated()),
            this, SLOT(slotActiveMainWindowChanged()));

    connect(notationView, SIGNAL(selectTrack(int)),
            this, SLOT(slotSelectTrackSegments(int)));

    connect(notationView, SIGNAL(play()),
            RosegardenMainWindow::self(), SLOT(slotPlay()));
    connect(notationView, SIGNAL(stop()),
            RosegardenMainWindow::self(), SLOT(slotStop()));
    connect(notationView, SIGNAL(fastForwardPlayback()),
            RosegardenMainWindow::self(), SLOT(slotFastforward()));
    connect(notationView, SIGNAL(rewindPlayback()),
            RosegardenMainWindow::self(), SLOT(slotRewind()));
    connect(notationView, SIGNAL(fastForwardPlaybackToEnd()),
            RosegardenMainWindow::self(), SLOT(slotFastForwardToEnd()));
    connect(notationView, SIGNAL(rewindPlaybackToBeginning()),
            RosegardenMainWindow::self(), SLOT(slotRewindToBeginning()));
    connect(notationView, SIGNAL(panic()),
            RosegardenMainWindow::self(), SLOT(slotPanic()));

    connect(notationView, SIGNAL(saveFile()),
            RosegardenMainWindow::self(), SLOT(slotFileSave()));
    connect(notationView, SIGNAL(openInNotation(std::vector<Segment *>)),
            this, SLOT(slotEditSegmentsNotation(std::vector<Segment *>)));
    connect(notationView, SIGNAL(openInMatrix(std::vector<Segment *>)),
            this, SLOT(slotEditSegmentsMatrix(std::vector<Segment *>)));
    connect(notationView, SIGNAL(openInPercussionMatrix(std::vector<Segment *>)),
            this, SLOT(slotEditSegmentsPercussionMatrix(std::vector<Segment *>)));
    connect(notationView, SIGNAL(openInEventList(std::vector<Segment *>)),
            this, SLOT(slotEditSegmentsEventList(std::vector<Segment *>)));
    connect(notationView, SIGNAL(editTriggerSegment(int)),
            this, SLOT(slotEditTriggerSegment(int)));
    // No such signal comes from NotationView
    //connect(notationView, SIGNAL(staffLabelChanged(TrackId, QString)),
    //        this, SLOT(slotChangeTrackLabel(TrackId, QString)));
    connect(notationView, SIGNAL(toggleSolo(bool)),
            RosegardenMainWindow::self(), SLOT(slotToggleSolo(bool)));
    //connect(notationView, SIGNAL(editTimeSignature(timeT)),
    //        RosegardenMainWindow::self(), SLOT(slotEditTempos(timeT)));

    SequenceManager *sM = getDocument()->getSequenceManager();

    connect(sM, SIGNAL(insertableNoteOnReceived(int, int)),
            notationView, SLOT(slotInsertableNoteOnReceived(int, int)));
    connect(sM, SIGNAL(insertableNoteOffReceived(int, int)),
            notationView, SLOT(slotInsertableNoteOffReceived(int, int)));

    connect(notationView, SIGNAL(stepByStepTargetRequested(QObject *)),
            this, SIGNAL(stepByStepTargetRequested(QObject *)));
    connect(this, SIGNAL(stepByStepTargetRequested(QObject *)),
            notationView, SLOT(slotStepByStepTargetRequested(QObject *)));
    connect(RosegardenMainWindow::self(), SIGNAL(compositionStateUpdate()),
            notationView, SLOT(slotCompositionStateUpdate()));
    connect(this, SIGNAL(compositionStateUpdate()),
            notationView, SLOT(slotCompositionStateUpdate()));

    // Encourage the notation view window to open to the same
    // interval as the current segment view.  Since scrollToTime is
    // commented out (what bug?), it made no sense to leave the
    // support code in.
//     if (m_trackEditor->getCompositionView()->horizontalScrollBar()->value() > 1) { // don't scroll unless we need to
//         // first find the time at the center of the visible segment canvas
//         int centerX = (int)(m_trackEditor->getCompositionView()->contentsX() +
//                             m_trackEditor->getCompositionView()->visibleWidth() / 2);
//         timeT centerSegmentView = m_trackEditor->getRulerScale()->getTimeForX(centerX);
//         // then scroll the notation view to that time, "localized" for the current segment
// //!!!        notationView->scrollToTime(centerSegmentView);
// //!!!        notationView->updateView();
//     }

    return notationView;
}

// mostly copied from slotEditSegmentNotationView, but some changes
// marked with CMT
void RosegardenMainViewWidget::slotEditSegmentPitchTracker(Segment* p)
{

    SetWaitCursor waitCursor;
    std::vector<Segment *> segmentsToEdit;

    RG_DEBUG << "\n\n\n\nRosegardenMainViewWidget::slotEditSegmentNotation: p is " << p << endl;

    // The logic here is: If we're calling for this operation to
    // happen on a particular segment, then open that segment and if
    // it's part of a selection open all other selected segments too.
    // If we're not calling for any particular segment, then open all
    // selected segments if there are any.

    if (haveSelection()) {

        SegmentSelection selection = getSelection();

        if (!p || (selection.find(p) != selection.end())) {
            for (SegmentSelection::iterator i = selection.begin();
                    i != selection.end(); ++i) {
                if ((*i)->getType() != Segment::Audio) {
                    segmentsToEdit.push_back(*i);
                }
            }
        } else {
            if (p->getType() != Segment::Audio) {
                segmentsToEdit.push_back(p);
            }
        }

    } else if (p) {
        if (p->getType() != Segment::Audio) {
            segmentsToEdit.push_back(p);
        }
    } else {
        return ;
    }

    if (segmentsToEdit.empty()) {
        /* was sorry */ QMessageBox::warning(this, "", tr("No non-audio segments selected"));
        return ;
    }


    // addition by CMT
    if (segmentsToEdit.size() > 1) {
        QMessageBox::warning(this, "", tr("Pitch Tracker can only contain 1 segment."));
        return ;
    }
    //!!! not necessary?  NotationView doesn't do this.  -gp
//    if (Segment::Audio == segmentsToEdit[0]->getType()) {
//        QMessageBox::warning(this, "", tr("Pitch Tracker needs a non-audio track."));
//        return ;
//    }

    slotEditSegmentsPitchTracker(segmentsToEdit);
}

void RosegardenMainViewWidget::slotEditSegmentsPitchTracker(std::vector<Segment *> segmentsToEdit)
{
    PitchTrackerView *view = createPitchTrackerView(segmentsToEdit);
    if (view) {
        if (view->getJackConnected()) {
            view->show();
        } else {
            delete view;
        }
    }
}

//!!! copied (+ renamed vars) blindly from NotationView, but it works.  -gp
PitchTrackerView *
RosegardenMainViewWidget::createPitchTrackerView(std::vector<Segment *> segmentsToEdit)
{
    PitchTrackerView *pitchTrackerView =
        new PitchTrackerView(getDocument(), segmentsToEdit, this);


    // For tempo changes (ugh -- it'd be nicer to make a tempo change
    // command that could interpret all this stuff from the dialog)
    //
    connect(pitchTrackerView, SIGNAL(changeTempo(timeT,
                                 tempoT,
                                 tempoT,
                                 TempoDialog::TempoDialogAction)),
            RosegardenMainWindow::self(), SLOT(slotChangeTempo(timeT,
                                           tempoT,
                                           tempoT,
                                           TempoDialog::TempoDialogAction)));


    connect(pitchTrackerView, SIGNAL(windowActivated()),
            this, SLOT(slotActiveMainWindowChanged()));

    connect(pitchTrackerView, SIGNAL(selectTrack(int)),
            this, SLOT(slotSelectTrackSegments(int)));

    connect(pitchTrackerView, SIGNAL(play()),
            RosegardenMainWindow::self(), SLOT(slotPlay()));
    connect(pitchTrackerView, SIGNAL(stop()),
            RosegardenMainWindow::self(), SLOT(slotStop()));
    connect(pitchTrackerView, SIGNAL(fastForwardPlayback()),
            RosegardenMainWindow::self(), SLOT(slotFastforward()));
    connect(pitchTrackerView, SIGNAL(rewindPlayback()),
            RosegardenMainWindow::self(), SLOT(slotRewind()));
    connect(pitchTrackerView, SIGNAL(fastForwardPlaybackToEnd()),
            RosegardenMainWindow::self(), SLOT(slotFastForwardToEnd()));
    connect(pitchTrackerView, SIGNAL(rewindPlaybackToBeginning()),
            RosegardenMainWindow::self(), SLOT(slotRewindToBeginning()));
    connect(pitchTrackerView, SIGNAL(panic()),
            RosegardenMainWindow::self(), SLOT(slotPanic()));

    connect(pitchTrackerView, SIGNAL(saveFile()),
            RosegardenMainWindow::self(), SLOT(slotFileSave()));
//  This probably is obsolete in Thorn.
//    connect(pitchTrackerView, SIGNAL(jumpPlaybackTo(timeT)),
//            getDocument(), SLOT(slotSetPointerPosition(timeT)));
    connect(pitchTrackerView, SIGNAL(openInNotation(std::vector<Segment *>)),
            this, SLOT(slotEditSegmentsNotation(std::vector<Segment *>)));
    connect(pitchTrackerView, SIGNAL(openInMatrix(std::vector<Segment *>)),
            this, SLOT(slotEditSegmentsMatrix(std::vector<Segment *>)));
    connect(pitchTrackerView, SIGNAL(openInPercussionMatrix(std::vector<Segment *>)),
            this, SLOT(slotEditSegmentsPercussionMatrix(std::vector<Segment *>)));
    connect(pitchTrackerView, SIGNAL(openInEventList(std::vector<Segment *>)),
            this, SLOT(slotEditSegmentsEventList(std::vector<Segment *>)));
/* hjj: WHAT DO DO WITH THIS ?
    connect(pitchTrackerView, SIGNAL(editMetadata(QString)),
            this, SLOT(slotEditMetadata(QString)));
*/
    connect(pitchTrackerView, SIGNAL(editTriggerSegment(int)),
            this, SLOT(slotEditTriggerSegment(int)));
    // No such signal comes from PitchTrackerView
    //connect(pitchTrackerView, SIGNAL(staffLabelChanged(TrackId, QString)),
    //        this, SLOT(slotChangeTrackLabel(TrackId, QString)));
    connect(pitchTrackerView, SIGNAL(toggleSolo(bool)),
            RosegardenMainWindow::self(), SLOT(slotToggleSolo(bool)));
    //connect(pitchTrackerView, SIGNAL(editTimeSignature(timeT)),
    //        RosegardenMainWindow::self(), SLOT(slotEditTempos(timeT)));

    SequenceManager *sM = getDocument()->getSequenceManager();

    connect(sM, SIGNAL(insertableNoteOnReceived(int, int)),
            pitchTrackerView, SLOT(slotInsertableNoteOnReceived(int, int)));
    connect(sM, SIGNAL(insertableNoteOffReceived(int, int)),
            pitchTrackerView, SLOT(slotInsertableNoteOffReceived(int, int)));

    connect(pitchTrackerView, SIGNAL(stepByStepTargetRequested(QObject *)),
            this, SIGNAL(stepByStepTargetRequested(QObject *)));
    connect(this, SIGNAL(stepByStepTargetRequested(QObject *)),
            pitchTrackerView, SLOT(slotStepByStepTargetRequested(QObject *)));
    connect(RosegardenMainWindow::self(), SIGNAL(compositionStateUpdate()),
            pitchTrackerView, SLOT(slotCompositionStateUpdate()));
    connect(this, SIGNAL(compositionStateUpdate()),
            pitchTrackerView, SLOT(slotCompositionStateUpdate()));

    // Encourage the notation view window to open to the same
    // interval as the current segment view.  Since scrollToTime is
    // commented out (what bug?), it made no sense to leave the
    // support code in.
//     if (m_trackEditor->getCompositionView()->horizontalScrollBar()->value() > 1) { // don't scroll unless we need to
//         // first find the time at the center of the visible segment canvas
//         int centerX = (int)(m_trackEditor->getCompositionView()->contentsX() +
//                             m_trackEditor->getCompositionView()->visibleWidth() / 2);
//         timeT centerSegmentView = m_trackEditor->getRulerScale()->getTimeForX(centerX);
//         // then scroll the notation view to that time, "localized" for the current segment
// //!!!        pitchTrackerView->scrollToTime(centerSegmentView);
// //!!!        pitchTrackerView->updateView();
//     }

    return pitchTrackerView;
}

void RosegardenMainViewWidget::slotEditSegmentMatrix(Segment* p)
{
    SetWaitCursor waitCursor;

    std::vector<Segment *> segmentsToEdit;

    if (haveSelection()) {

        SegmentSelection selection = getSelection();

        if (!p || (selection.find(p) != selection.end())) {
            for (SegmentSelection::iterator i = selection.begin();
                    i != selection.end(); ++i) {
                if ((*i)->getType() != Segment::Audio) {
                    segmentsToEdit.push_back(*i);
                }
            }
        } else {
            if (p->getType() != Segment::Audio) {
                segmentsToEdit.push_back(p);
            }
        }

    } else if (p) {
        if (p->getType() != Segment::Audio) {
            segmentsToEdit.push_back(p);
        }
    } else {
        return ;
    }

/*!!!
    // unlike notation, if we're calling for this on a particular
    // segment we don't open all the other selected segments as well
    // (fine in notation because they're in a single window)

    if (p) {
        if (p->getType() != Segment::Audio) {
            segmentsToEdit.push_back(p);
        }
    } else {
        int count = 0;
        SegmentSelection selection = getSelection();
        for (SegmentSelection::iterator i = selection.begin();
                i != selection.end(); ++i) {
            if ((*i)->getType() != Segment::Audio) {
                slotEditSegmentMatrix(*i);
                if (++count == maxEditorsToOpen)
                    break;
            }
        }
        return ;
    }
*/
    if (segmentsToEdit.empty()) {
         QMessageBox::warning(this, tr("Rosegarden"), tr("No non-audio segments selected"));
        return ;
    }

    slotEditSegmentsMatrix(segmentsToEdit);
}

void RosegardenMainViewWidget::slotEditSegmentPercussionMatrix(Segment* p)
{
    SetWaitCursor waitCursor;

    std::vector<Segment *> segmentsToEdit;

    if (haveSelection()) {

        SegmentSelection selection = getSelection();

        if (!p || (selection.find(p) != selection.end())) {
            for (SegmentSelection::iterator i = selection.begin();
                    i != selection.end(); ++i) {
                if ((*i)->getType() != Segment::Audio) {
                    segmentsToEdit.push_back(*i);
                }
            }
        } else {
            if (p->getType() != Segment::Audio) {
                segmentsToEdit.push_back(p);
            }
        }

    } else if (p) {
        if (p->getType() != Segment::Audio) {
            segmentsToEdit.push_back(p);
        }
    } else {
        return ;
    }

    if (segmentsToEdit.empty()) {
         QMessageBox::warning(this, tr("Rosegarden"), tr("No non-audio segments selected"));
        return ;
    }

    slotEditSegmentsPercussionMatrix(segmentsToEdit);
}

void RosegardenMainViewWidget::slotEditSegmentsMatrix(std::vector<Segment *> segmentsToEdit)
{
    MatrixView *view = createMatrixView(segmentsToEdit, false);
    if (view) view->show();
}

void RosegardenMainViewWidget::slotEditSegmentsPercussionMatrix(std::vector<Segment *> segmentsToEdit)
{
    MatrixView *view = createMatrixView(segmentsToEdit, true);
    if (view) view->show();

}

MatrixView *
RosegardenMainViewWidget::createMatrixView(std::vector<Segment *> segmentsToEdit, bool drumMode)
{
    MatrixView *matrixView = new MatrixView(getDocument(),
                                                  segmentsToEdit,
                                                  drumMode,
                                                  this);

    // For tempo changes (ugh -- it'd be nicer to make a tempo change
    // command that could interpret all this stuff from the dialog)
    //
    connect(matrixView, SIGNAL(changeTempo(timeT,
                                           tempoT,
                                           tempoT,
                                           TempoDialog::TempoDialogAction)),
            RosegardenMainWindow::self(), SLOT(slotChangeTempo(timeT,
                                           tempoT,
                                           tempoT,
                                           TempoDialog::TempoDialogAction)));

    connect(matrixView, SIGNAL(windowActivated()),
            this, SLOT(slotActiveMainWindowChanged()));

    connect(matrixView, SIGNAL(selectTrack(int)),
            this, SLOT(slotSelectTrackSegments(int)));

    connect(matrixView, SIGNAL(play()),
            RosegardenMainWindow::self(), SLOT(slotPlay()));
    connect(matrixView, SIGNAL(stop()),
            RosegardenMainWindow::self(), SLOT(slotStop()));
    connect(matrixView, SIGNAL(fastForwardPlayback()),
            RosegardenMainWindow::self(), SLOT(slotFastforward()));
    connect(matrixView, SIGNAL(rewindPlayback()),
            RosegardenMainWindow::self(), SLOT(slotRewind()));
    connect(matrixView, SIGNAL(fastForwardPlaybackToEnd()),
            RosegardenMainWindow::self(), SLOT(slotFastForwardToEnd()));
    connect(matrixView, SIGNAL(rewindPlaybackToBeginning()),
            RosegardenMainWindow::self(), SLOT(slotRewindToBeginning()));
    connect(matrixView, SIGNAL(panic()),
            RosegardenMainWindow::self(), SLOT(slotPanic()));

    connect(matrixView, SIGNAL(saveFile()),
            RosegardenMainWindow::self(), SLOT(slotFileSave()));
    connect(matrixView, SIGNAL(openInNotation(std::vector<Segment *>)),
            this, SLOT(slotEditSegmentsNotation(std::vector<Segment *>)));
    connect(matrixView, SIGNAL(openInMatrix(std::vector<Segment *>)),
            this, SLOT(slotEditSegmentsMatrix(std::vector<Segment *>)));
    connect(matrixView, SIGNAL(openInPercussionMatrix(std::vector<Segment *>)),
            this, SLOT(slotEditSegmentsPercussionMatrix(std::vector<Segment *>)));
    connect(matrixView, SIGNAL(openInEventList(std::vector<Segment *>)),
            this, SLOT(slotEditSegmentsEventList(std::vector<Segment *>)));
    connect(matrixView, SIGNAL(editTriggerSegment(int)),
            this, SLOT(slotEditTriggerSegment(int)));
    connect(matrixView, SIGNAL(toggleSolo(bool)),
            RosegardenMainWindow::self(), SLOT(slotToggleSolo(bool)));
    //connect(matrixView, SIGNAL(editTimeSignature(timeT)),
    //        RosegardenMainWindow::self(), SLOT(slotEditTempos(timeT)));

    SequenceManager *sM = getDocument()->getSequenceManager();

    connect(sM, SIGNAL(insertableNoteOnReceived(int, int)),
            matrixView, SLOT(slotInsertableNoteOnReceived(int, int)));
    connect(sM, SIGNAL(insertableNoteOffReceived(int, int)),
            matrixView, SLOT(slotInsertableNoteOffReceived(int, int)));

    connect(matrixView, SIGNAL(stepByStepTargetRequested(QObject *)),
            this, SIGNAL(stepByStepTargetRequested(QObject *)));
    connect(this, SIGNAL(stepByStepTargetRequested(QObject *)),
            matrixView, SLOT(slotStepByStepTargetRequested(QObject *)));
    connect(RosegardenMainWindow::self(), SIGNAL(compositionStateUpdate()),
            matrixView, SLOT(slotCompositionStateUpdate()));
    connect(this, SIGNAL(compositionStateUpdate()),
            matrixView, SLOT(slotCompositionStateUpdate()));

    // Encourage the matrix view window to open to the same
    // interval as the current segment view.   Since scrollToTime is
    // commented out (what bug?), it made no sense to leave the
    // support code in.
//     if (m_trackEditor->getCompositionView()->horizontalScrollBar()->value() > 1) { // don't scroll unless we need to
//         // first find the time at the center of the visible segment canvas
//         int centerX = (int)(m_trackEditor->getCompositionView()->contentsX());
//         // Seems to work better for matrix view to scroll to left side
//         // + m_trackEditor->getCompositionView()->visibleWidth() / 2);
//         timeT centerSegmentView = m_trackEditor->getRulerScale()->getTimeForX(centerX);
//         // then scroll the view to that time, "localized" for the current segment
// //!!!        matrixView->scrollToTime(centerSegmentView);
// //!!!        matrixView->updateView();
//     }

    return matrixView;
}

void RosegardenMainViewWidget::slotEditSegmentEventList(Segment *p)
{
    SetWaitCursor waitCursor;

    std::vector<Segment *> segmentsToEdit;

    // unlike notation, if we're calling for this on a particular
    // segment we don't open all the other selected segments as well
    // (fine in notation because they're in a single window)

    if (p) {
        if (p->getType() != Segment::Audio) {
            segmentsToEdit.push_back(p);
        }
    } else {
        int count = 0;
        SegmentSelection selection = getSelection();
        for (SegmentSelection::iterator i = selection.begin();
                i != selection.end(); ++i) {
            if ((*i)->getType() != Segment::Audio) {
                slotEditSegmentEventList(*i);
                if (++count == maxEditorsToOpen)
                    break;
            }
        }
        return ;
    }

    if (segmentsToEdit.empty()) {
         QMessageBox::warning(this, tr("Rosegarden"), tr("No non-audio segments selected"));
        return ;
    }

    slotEditSegmentsEventList(segmentsToEdit);
}

void RosegardenMainViewWidget::slotEditSegmentsEventList(std::vector<Segment *> segmentsToEdit)
{
    int count = 0;
    for (std::vector<Segment *>::iterator i = segmentsToEdit.begin();
            i != segmentsToEdit.end(); ++i) {
        std::vector<Segment *> tmpvec;
        tmpvec.push_back(*i);
        EventView *view = createEventView(tmpvec);
        if (view) {
            view->show();
            if (++count == maxEditorsToOpen)
                break;
        }
    }
}

void RosegardenMainViewWidget::slotEditTriggerSegment(int id)
{
    std::cerr << "RosegardenMainViewWidget caught editTriggerSegment signal" << std::endl;
    SetWaitCursor waitCursor;

    std::vector<Segment *> segmentsToEdit;

    Segment *s = getDocument()->getComposition().getTriggerSegment(id);

    if (s) {
        segmentsToEdit.push_back(s);
    } else {
        std::cerr << "caught id: " << id << " and must not have been valid?" << std::endl;
        return ;
    }

    slotEditSegmentsEventList(segmentsToEdit);
}

void RosegardenMainViewWidget::slotSegmentAutoSplit(Segment *segment)
{
    AudioSplitDialog aSD(this, segment, getDocument());

    if (aSD.exec() == QDialog::Accepted) {
        Command *command =
            new AudioSegmentAutoSplitCommand(getDocument(),
                                             segment, aSD.getThreshold());
        slotAddCommandToHistory(command);
    }
}

void RosegardenMainViewWidget::slotEditSegmentAudio(Segment *segment)
{
    std::cout << "RosegardenMainViewWidget::slotEditSegmentAudio() - "
    << "starting external audio editor" << std::endl;

    QSettings settings;
    settings.beginGroup( GeneralOptionsConfigGroup );

    QString application = settings.value("externalaudioeditor", "").toString();
    settings.endGroup();

    if (application == "") {
        application = AudioConfigurationPage::getBestAvailableAudioEditor();
    }

    QStringList splitCommand = application.split(" ", QString::SkipEmptyParts);

    if (splitCommand.size() == 0) {

        std::cerr << "RosegardenMainViewWidget::slotEditSegmentAudio() - "
        << "external editor \"" << application.data()
        << "\" not found" << std::endl;

         QMessageBox::warning(this, tr("Rosegarden"), 
                           tr("You've not yet defined an audio editor for Rosegarden to use.\nSee Edit -> Preferences -> Audio."));

        return ;
    }

    QFileInfo *appInfo = new QFileInfo(splitCommand[0]);
    if (appInfo->exists() == false || appInfo->isExecutable() == false) {
        std::cerr << "RosegardenMainViewWidget::slotEditSegmentAudio() - "
                  << "can't execute \"" << splitCommand[0] << "\""
                  << std::endl;
        return;
    }

    AudioFile *aF = getDocument()->getAudioFileManager().
                    getAudioFile(segment->getAudioFileId());
    if (aF == 0) {
        std::cerr << "RosegardenMainViewWidget::slotEditSegmentAudio() - "
        << "can't find audio file" << std::endl;
        return ;
    }

    // wait cursor
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    // Setup the process
    //
    QProcess *process = new QProcess();
    splitCommand << aF->getFilename();

    // Start it
    //
    process->start(splitCommand.takeFirst(), splitCommand);
    if (!process->waitForStarted()) {  //@@@ JAS Check here first for errors
        std::cerr << "RosegardenMainViewWidget::slotEditSegmentAudio() - "
        << "can't start external editor" << std::endl;
    }

    // restore cursor
    QApplication::restoreOverrideCursor();

}

void RosegardenMainViewWidget::setZoomSize(double size)
{
    double oldSize = m_rulerScale->getUnitsPerPixel();

    // For readability
    CompositionView *compositionView = m_trackEditor->getCompositionView();
    
    QScrollBar *horizScrollBar = compositionView->horizontalScrollBar();
    int halfWidth = lround(compositionView->viewport()->width() / 2.0);
    int oldHCenter = horizScrollBar->value() + halfWidth;

    // Set the new zoom factor
    m_rulerScale->setUnitsPerPixel(size);

#if 0
    double duration44 = TimeSignature(4, 4).getBarDuration();
    double xScale = duration44 / (size * barWidth44);
    RG_DEBUG << "RosegardenMainViewWidget::setZoomSize():  xScale = " << 
        xScale;
#endif

    // Redraw everything

    // Redraw the playback position pointer.
    // Move these lines to a new CompositionView::redrawPointer()?
    timeT pointerTime = getDocument()->getComposition().getPosition();
    double pointerXPosition = compositionView->
        grid().getRulerScale()->getXForTime(pointerTime);
    compositionView->drawPointer(pointerXPosition);

    compositionView->deleteCachedPreviews();
    compositionView->slotUpdateSize();
    compositionView->slotUpdateAll();

    // At this point, the scroll bar's range has been updated.
    // We can now safely modify it.
    
    // Maintain the center of the view.
    // ??? See MatrixWidget and NotationWidget for a more extensive 
    //   zoom/panner feature.
    horizScrollBar->setValue(
        (int)(oldHCenter * (oldSize / size)) - halfWidth);
    
    // ??? An alternate behavior is to have the zoom always center on the
    //   playback position pointer.  Might make this a user preference, or
    //   maybe when holding down "Shift" while zooming.
//    horizScrollBar->setValue(pointerXPosition - halfWidth);

    if (m_trackEditor->getTempoRuler())
        m_trackEditor->getTempoRuler()->repaint();

    if (m_trackEditor->getChordNameRuler())
        m_trackEditor->getChordNameRuler()->repaint();

    if (m_trackEditor->getTopStandardRuler())
        m_trackEditor->getTopStandardRuler()->repaint();

    if (m_trackEditor->getBottomStandardRuler())
        m_trackEditor->getBottomStandardRuler()->repaint();
}

void RosegardenMainViewWidget::slotSelectTrackSegments(int trackId)
{
    Composition &comp = getDocument()->getComposition();
    Track *track = comp.getTrackById(trackId);

    if (track == 0)
        return ;

    SegmentSelection segments;

    if (QApplication::keyboardModifiers() != Qt::ShiftModifier) {
      
        // Shift key is not pressed :
        
        // Select all segments on the current track
        // (all the other segments will be deselected)
        for (Composition::iterator i =
                    getDocument()->getComposition().begin();
                i != getDocument()->getComposition().end(); ++i) {
            if (((int)(*i)->getTrack()) == trackId)
                segments.insert(*i);
        }
      
    } else {

        // Shift key is pressed :

        // Get the list of the currently selected segments 
        segments = getSelection();

        // Segments on the current track will be added to or removed
        // from this list depending of the number of segments already
        // selected on this track.

        // Look for already selected segments on this track
        bool noSegSelected = true;
        for (Composition::iterator i =
                  getDocument()->getComposition().begin();
              i != getDocument()->getComposition().end(); ++i) {
            if (((int)(*i)->getTrack()) == trackId) {
                if (segments.count(*i)) {
                    // Segment *i is selected
                    noSegSelected = false;
                }
            }
        }

        if (!noSegSelected) {

            // Some segments are selected :
            // Deselect all selected segments on this track
            for (Composition::iterator i =
                     getDocument()->getComposition().begin();
                 i != getDocument()->getComposition().end(); ++i) {
                if (((int)(*i)->getTrack()) == trackId) {
                    if (segments.count(*i)) {
                        // Segment *i is selected
                        segments.erase(*i);
                    }
                }
            }
            

        } else {
        
            // There is no selected segment on this track :
            // Select all segments on this track
            for (Composition::iterator i =
                     getDocument()->getComposition().begin();
                 i != getDocument()->getComposition().end(); ++i) {
                if (((int)(*i)->getTrack()) == trackId) {
                    segments.insert(*i);
                }
            }
        }
        
    }
    

    // This is now handled via Composition::notifyTrackSelectionChanged()
    //m_trackEditor->getTrackButtons()->selectTrack(track->getPosition());

    // Make sure the track is visible.
    m_trackEditor->slotScrollToTrack(track->getPosition());

    // Store the selected Track in the Composition
    //
    comp.setSelectedTrack(trackId);

    m_trackParameterBox->slotSelectedTrackChanged();

    // update the instrument parameter box
    slotUpdateInstrumentParameterBox(comp.getTrackById(trackId)->
                                     getInstrument());


    slotPropagateSegmentSelection(segments);

    // inform
    emit segmentsSelected(segments);
    emit compositionStateUpdate();
}

void RosegardenMainViewWidget::slotPropagateSegmentSelection(const SegmentSelection &segments)
{
    // Send this signal to the GUI to activate the correct tool
    // on the toolbar so that we have a SegmentSelector object
    // to write the Segments into
    //
    if (!segments.empty()) {
        emit activateTool(SegmentSelector::ToolName);
    }

    // Send the segment list even if it's empty as we
    // use that to clear any current selection
    //
    m_trackEditor->getCompositionView()->selectSegments(segments);

    // update the segment parameter box
    m_segmentParameterBox->useSegments(segments);

    if (!segments.empty()) {
        emit stateChange("have_selection", true);
        if (!hasNonAudioSegment(segments)) {
            emit stateChange("audio_segment_selected", true);
        }
    } else {
        emit stateChange("have_selection", false);
    }
}

void RosegardenMainViewWidget::slotSelectAllSegments()
{
    SegmentSelection segments;

    InstrumentId instrument = 0;
    bool haveInstrument = false;
    bool multipleInstruments = false;

    Composition &comp = getDocument()->getComposition();

    for (Composition::iterator i = comp.begin(); i != comp.end(); ++i) {

        InstrumentId myInstrument =
            comp.getTrackById((*i)->getTrack())->getInstrument();

        if (haveInstrument) {
            if (myInstrument != instrument) {
                multipleInstruments = true;
            }
        } else {
            instrument = myInstrument;
            haveInstrument = true;
        }

        segments.insert(*i);
    }

    // Send this signal to the GUI to activate the correct tool
    // on the toolbar so that we have a SegmentSelector object
    // to write the Segments into
    //
    if (!segments.empty()) {
        emit activateTool(SegmentSelector::ToolName);
    }

    // Send the segment list even if it's empty as we
    // use that to clear any current selection
    //
    m_trackEditor->getCompositionView()->selectSegments(segments);

    // update the segment parameter box
    m_segmentParameterBox->useSegments(segments);

    // update the instrument parameter box
    if (haveInstrument && !multipleInstruments) {
        slotUpdateInstrumentParameterBox(instrument);
    } else {
        m_instrumentParameterBox->useInstrument(0);
    }

    //!!! similarly, how to set no selected track?
    //comp.setSelectedTrack(trackId);

    if (!segments.empty()) {
        emit stateChange("have_selection", true);
        if (!hasNonAudioSegment(segments)) {
            emit stateChange("audio_segment_selected", true);
        }
    } else {
        emit stateChange("have_selection", false);
    }

    // inform
    //!!! inform what? is this signal actually used?
    emit segmentsSelected(segments);
}

void RosegardenMainViewWidget::slotUpdateInstrumentParameterBox(int id)
{
    Studio &studio = getDocument()->getStudio();
    Instrument *instrument = studio.getInstrumentById(id);
    // Composition &comp = getDocument()->getComposition();

    // Track *track = comp.getTrackById(comp.getSelectedTrack());

    // Reset the instrument
    //
    m_instrumentParameterBox->useInstrument(instrument);
    
    // set prog-change select-box unchecked (if selected TrackChanged)
    MIDIInstrumentParameterPanel *mipp;
    mipp = m_instrumentParameterBox->getMIDIInstrumentParameterPanel();
    mipp->clearReceiveExternal();
    
    // Then do this instrument/track fiddling
    //
    /*
        if (track && instrument &&
                instrument->getType() == Instrument::Audio)
        {
            // Set the mute status
            m_instrumentParameterBox->setMute(track->isMuted());
     
            // Set the record track
            m_instrumentParameterBox->setRecord(
                        track->getId() == comp.getRecordTrack());
     
            // Set solo
            m_instrumentParameterBox->setSolo(
                    comp.isSolo() && (track->getId() == comp.getSelectedTrack()));
        }
    */
    emit checkTrackAssignments();
}

void RosegardenMainViewWidget::showVisuals(const MappedEvent *mE)
{
    double valueLeft = ((double)mE->getData1()) / 127.0;
    double valueRight = ((double)mE->getData2()) / 127.0;

    if (mE->getType() == MappedEvent::AudioLevel) {

        // Send to the high sensitivity instrument parameter box
        // (if any)
        //
        if (m_instrumentParameterBox->getSelectedInstrument() &&
                mE->getInstrument() ==
                m_instrumentParameterBox->getSelectedInstrument()->getId()) {
            float dBleft = AudioLevel::fader_to_dB
                           (mE->getData1(), 127, AudioLevel::LongFader);
            float dBright = AudioLevel::fader_to_dB
                            (mE->getData2(), 127, AudioLevel::LongFader);

            m_instrumentParameterBox->setAudioMeter(dBleft, dBright,
                                                    AudioLevel::DB_FLOOR,
                                                    AudioLevel::DB_FLOOR);
        }

        // Don't always send all audio levels so we don't
        // get vu meter flickering on track meters
        //
        if (valueLeft < 0.05 && valueRight < 0.05)
            return ;

    } else if (mE->getType() != MappedEvent::MidiNote)
        return ;

    m_trackEditor->getTrackButtons()->
    slotSetMetersByInstrument((valueLeft + valueRight) / 2,
                              mE->getInstrument());

}

void
RosegardenMainViewWidget::updateMeters()
{
    const int unknownState = 0, oldState = 1, newState = 2;

    typedef std::map<InstrumentId, int> StateMap;
    static StateMap states;
    static StateMap recStates;

    typedef std::map<InstrumentId, LevelInfo> LevelMap;
    static LevelMap levels;
    static LevelMap recLevels;

    for (StateMap::iterator i = states.begin(); i != states.end(); ++i) {
        i->second = unknownState;
    }
    for (StateMap::iterator i = recStates.begin(); i != recStates.end(); ++i) {
        i->second = unknownState;
    }

    for (Composition::trackcontainer::iterator i =
             getDocument()->getComposition().getTracks().begin();
         i != getDocument()->getComposition().getTracks().end(); ++i) {

        Track *track = i->second;
        if (!track) continue;

        InstrumentId instrumentId = track->getInstrument();

        if (states[instrumentId] == unknownState) {
            bool isNew =
                SequencerDataBlock::getInstance()->getInstrumentLevel
                (instrumentId, levels[instrumentId]);
            states[instrumentId] = (isNew ? newState : oldState);
        }

        if (recStates[instrumentId] == unknownState) {
            bool isNew =
                SequencerDataBlock::getInstance()->getInstrumentRecordLevel
                (instrumentId, recLevels[instrumentId]);
            recStates[instrumentId] = (isNew ? newState : oldState);
        }

        if (states[instrumentId] == oldState &&
            recStates[instrumentId] == oldState)
            continue;

        Instrument *instrument =
            getDocument()->getStudio().getInstrumentById(instrumentId);
        if (!instrument) continue;

        // This records the level of this instrument, not necessarily
        // caused by notes on this particular track.
        LevelInfo &info = levels[instrumentId];
        LevelInfo &recInfo = recLevels[instrumentId];

        if (instrument->getType() == Instrument::Audio ||
            instrument->getType() == Instrument::SoftSynth) {

            float dBleft = AudioLevel::DB_FLOOR;
            float dBright = AudioLevel::DB_FLOOR;
            float recDBleft = AudioLevel::DB_FLOOR;
            float recDBright = AudioLevel::DB_FLOOR;

            bool toSet = false;

            if (states[instrumentId] == newState &&
                (getDocument()->getSequenceManager()->getTransportStatus()
                 != STOPPED)) {

                if (info.level != 0 || info.levelRight != 0) {
                    dBleft = AudioLevel::fader_to_dB
                             (info.level, 127, AudioLevel::LongFader);
                    dBright = AudioLevel::fader_to_dB
                              (info.levelRight, 127, AudioLevel::LongFader);
                }
                toSet = true;
                m_trackEditor->getTrackButtons()->slotSetTrackMeter
                ((info.level + info.levelRight) / 254.0, track->getPosition());
            }

            if (recStates[instrumentId] == newState &&
                instrument->getType() == Instrument::Audio &&
                (getDocument()->getSequenceManager()->getTransportStatus()
                 != PLAYING)) {

                if (recInfo.level != 0 || recInfo.levelRight != 0) {
                    recDBleft = AudioLevel::fader_to_dB
                                (recInfo.level, 127, AudioLevel::LongFader);
                    recDBright = AudioLevel::fader_to_dB
                                 (recInfo.levelRight, 127, AudioLevel::LongFader);
                }
                toSet = true;
            }

            if (toSet &&
                m_instrumentParameterBox->getSelectedInstrument() &&
                instrument->getId() ==
                m_instrumentParameterBox->getSelectedInstrument()->getId()) {

                m_instrumentParameterBox->setAudioMeter(dBleft, dBright,
                                                        recDBleft, recDBright);
            }

        } else {
            // Not audio or softsynth
            if (info.level == 0)
                continue;

            if (getDocument()->getSequenceManager()->getTransportStatus()
                != STOPPED) {

                // The information in 'info' is specific for this instrument, not
                //  for this track.
                //m_trackEditor->getTrackButtons()->slotSetTrackMeter
                //	(info.level / 127.0, track->getPosition());
                m_trackEditor->getTrackButtons()->slotSetMetersByInstrument
                	(info.level / 127.0, instrumentId);
            }
        }
    }

    for (StateMap::iterator i = states.begin(); i != states.end(); ++i) {
        if (i->second == newState) {
            emit instrumentLevelsChanged(i->first, levels[i->first]);
        }
    }
}

void
RosegardenMainViewWidget::updateMonitorMeters()
{
    Instrument *instrument =
        m_instrumentParameterBox->getSelectedInstrument();
    if (!instrument ||
        (instrument->getType() != Instrument::Audio)) {
        return;
    }

    LevelInfo level;
    if (!SequencerDataBlock::getInstance()->
        getInstrumentRecordLevel(instrument->getId(), level)) {
        return;
    }

    float dBleft = AudioLevel::fader_to_dB
                   (level.level, 127, AudioLevel::LongFader);
    float dBright = AudioLevel::fader_to_dB
                    (level.levelRight, 127, AudioLevel::LongFader);

    m_instrumentParameterBox->setAudioMeter
    (AudioLevel::DB_FLOOR, AudioLevel::DB_FLOOR,
     dBleft, dBright);
}

void
RosegardenMainViewWidget::slotSelectedSegments(const SegmentSelection &segments)
{
    // update the segment parameter box
    m_segmentParameterBox->useSegments(segments);

    if (!segments.empty()) {
        emit stateChange("have_selection", true);
        if (!hasNonAudioSegment(segments))
            emit stateChange("audio_segment_selected", true);
    } else {
        emit stateChange("have_selection", false);
    }

    emit segmentsSelected(segments);
}

void RosegardenMainViewWidget::slotShowRulers(bool v)
{
    if (v) {
        m_trackEditor->getTopStandardRuler()->getLoopRuler()->show();
        m_trackEditor->getBottomStandardRuler()->getLoopRuler()->show();
    } else {
        m_trackEditor->getTopStandardRuler()->getLoopRuler()->hide();
        m_trackEditor->getBottomStandardRuler()->getLoopRuler()->hide();
    }
}

void RosegardenMainViewWidget::slotShowTempoRuler(bool v)
{
    if (v) {
        m_trackEditor->getTempoRuler()->show();
    } else {
        m_trackEditor->getTempoRuler()->hide();
    }
}

void RosegardenMainViewWidget::slotShowChordNameRuler(bool v)
{
    if (v) {
        m_trackEditor->getChordNameRuler()->setStudio(&getDocument()->getStudio());
        m_trackEditor->getChordNameRuler()->show();
    } else {
        m_trackEditor->getChordNameRuler()->hide();
    }
}

void RosegardenMainViewWidget::slotShowPreviews(bool v)
{
    m_trackEditor->getCompositionView()->setShowPreviews(v);
    m_trackEditor->getCompositionView()->slotUpdateAll();
}

void RosegardenMainViewWidget::slotShowSegmentLabels(bool v)
{
    m_trackEditor->getCompositionView()->setShowSegmentLabels(v);
    m_trackEditor->getCompositionView()->slotUpdateAll();
}

void RosegardenMainViewWidget::slotAddTracks(unsigned int nbTracks,
                                      InstrumentId id, int pos)
{
    RG_DEBUG << "RosegardenMainViewWidget::slotAddTracks(" << nbTracks << ", " << pos << ")" << endl;
    m_trackEditor->addTracks(nbTracks, id, pos);
}

void RosegardenMainViewWidget::slotDeleteTracks(
    std::vector<TrackId> tracks)
{
    RG_DEBUG << "RosegardenMainViewWidget::slotDeleteTracks - "
    << "deleting " << tracks.size() << " tracks"
    << endl;

    m_trackEditor->deleteTracks(tracks);
}

void
RosegardenMainViewWidget::slotAddCommandToHistory(Command *command)
{
    CommandHistory::getInstance()->addCommand(command);
}

#if 0
void
RosegardenMainViewWidget::slotChangeTrackLabel(TrackId id,
                                        QString label)
{
    m_trackEditor->getTrackButtons()->changeTrackName(id, label);
}
#endif

void
RosegardenMainViewWidget::slotAddAudioSegment(AudioFileId audioId,
                                       TrackId trackId,
                                       timeT position,
                                       const RealTime &startTime,
                                       const RealTime &endTime)
{
    AudioSegmentInsertCommand *command =
        new AudioSegmentInsertCommand(getDocument(),
                                      trackId,
                                      position,
                                      audioId,
                                      startTime,
                                      endTime);
    slotAddCommandToHistory(command);

    Segment *newSegment = command->getNewSegment();
    if (newSegment) {
        SegmentSelection selection;
        selection.insert(newSegment);
        slotPropagateSegmentSelection(selection);
        emit segmentsSelected(selection);
    }
}

void
RosegardenMainViewWidget::slotAddAudioSegmentCurrentPosition(AudioFileId audioFileId,
        const RealTime &startTime,
        const RealTime &endTime)
{
    std::cerr << "RosegardenMainViewWidget::slotAddAudioSegmentCurrentPosition(...) - slot firing as ordered, sir!" << std::endl;
    Composition &comp = getDocument()->getComposition();

    AudioSegmentInsertCommand *command =
        new AudioSegmentInsertCommand(getDocument(),
                                      comp.getSelectedTrack(),
                                      comp.getPosition(),
                                      audioFileId,
                                      startTime,
                                      endTime);
    slotAddCommandToHistory(command);

    Segment *newSegment = command->getNewSegment();
    if (newSegment) {
        SegmentSelection selection;
        selection.insert(newSegment);
        slotPropagateSegmentSelection(selection);
        emit segmentsSelected(selection);
    }
}

void
RosegardenMainViewWidget::slotAddAudioSegmentDefaultPosition(AudioFileId audioFileId,
        const RealTime &startTime,
        const RealTime &endTime)
{
    RG_DEBUG << "RosegardenMainViewWidget::slotAddAudioSegmentDefaultPosition()..." << endl;

    // Add at current track if it's an audio track, otherwise at first
    // empty audio track if there is one, otherwise at first audio track.
    // This behaviour should be of no interest to proficient users (who
    // should have selected the right track already, or be using drag-
    // and-drop) but it should save beginners from inserting an audio
    // segment and being quite unable to work out why it won't play

    Composition &comp = getDocument()->getComposition();
    Studio &studio = getDocument()->getStudio();

    TrackId currentTrackId = comp.getSelectedTrack();
    Track *track = comp.getTrackById(currentTrackId);

    if (track) {
        InstrumentId ii = track->getInstrument();
        Instrument *instrument = studio.getInstrumentById(ii);

        if (instrument &&
                instrument->getType() == Instrument::Audio) {
            slotAddAudioSegment(audioFileId, currentTrackId,
                                comp.getPosition(), startTime, endTime);
            return ;
        }
    }

    // current track is not an audio track, find a more suitable one

    TrackId bestSoFar = currentTrackId;

    for (Composition::trackcontainer::const_iterator
            ti = comp.getTracks().begin();
            ti != comp.getTracks().end(); ++ti) {

        InstrumentId ii = ti->second->getInstrument();
        Instrument *instrument = studio.getInstrumentById(ii);

        if (instrument &&
                instrument->getType() == Instrument::Audio) {

            if (bestSoFar == currentTrackId)
                bestSoFar = ti->first;
            bool haveSegment = false;

            for (SegmentMultiSet::const_iterator si =
                        comp.getSegments().begin();
                    si != comp.getSegments().end(); ++si) {
                if ((*si)->getTrack() == ti->first) {
                    // there's a segment on this track
                    haveSegment = true;
                    break;
                }
            }

            if (!haveSegment) { // perfect
                slotAddAudioSegment(audioFileId, ti->first,
                                    comp.getPosition(), startTime, endTime);
                return ;
            }
        }
    }

    slotAddAudioSegment(audioFileId, bestSoFar,
                        comp.getPosition(), startTime, endTime);
    return ;
}

void
RosegardenMainViewWidget::slotDroppedNewAudio(QString audioDesc)
{
    // If audio is not OK
    if (getDocument()->getSequenceManager()  &&
        !(getDocument()->getSequenceManager()->getSoundDriverStatus() & 
          AUDIO_OK)) {

#ifdef HAVE_LIBJACK
        QMessageBox::warning(this, tr("Rosegarden"), 
            tr("Cannot add dropped file.  JACK audio server is not available."));
#else
        QMessageBox::warning(this, tr("Rosegarden"), 
            tr("Cannot add dropped file.  This version of rosegarden was not built with audio support."));
#endif

        return;
    }

    QTextStream s(&audioDesc, QIODevice::ReadWrite); //### use QIODevice::ReadOnly instead ?
    
    QString url;
    int trackId;
    timeT time;
    url = s.readLine();
    s >> trackId;
    s >> time;

    std::cerr << "RosegardenMainViewWidget::slotDroppedNewAudio: url " << url << ", trackId " << trackId << ", time " << time << std::endl;

    // RosegardenMainWindow *mainWindow = RosegardenMainWindow::self();
    AudioFileManager &aFM = getDocument()->getAudioFileManager();

    AudioFileId audioFileId = 0;

    int sampleRate = 0;
    if (getDocument()->getSequenceManager()) {
        sampleRate = getDocument()->getSequenceManager()->getSampleRate();
    }

    QUrl qurl(url);
    if (!RosegardenMainWindow::self()->testAudioPath(tr("importing an audio file that needs to be converted or resampled"))) {
        return;
    }

    // from qt4-doc : " don't use a (modal) QProgressDialog inside a paintEvent() !"

    //cc 20150508: because ProgressDialog has WA_DeleteOnClose set, it
    // is destroyed if the user closes it during event processing --
    // use a QPointer to avoid dereferencing it afterwards
    QPointer<ProgressDialog> progressDlg =
        new ProgressDialog(tr("Adding audio file..."),
                           (QWidget*)this);
    progressDlg->setIndeterminate(true);
    
    // warning: pointer &progressDlg becomes invalid, if function quits.
    CurrentProgressDialog::set(progressDlg);
    
    // Connect the progress dialog
    //
    connect(&aFM, SIGNAL(setValue(int)),
            progressDlg, SLOT(setValue(int)));
    connect(&aFM, SIGNAL(setOperationName(QString)),
            progressDlg, SLOT(setLabelText(QString)));
    connect(progressDlg, SIGNAL(canceled()),
            &aFM, SLOT(slotStopImport()));

    try {
        audioFileId = aFM.importURL(qurl, sampleRate);
    } catch (AudioFileManager::BadAudioPathException e) {
        CurrentProgressDialog::freeze();
        if (progressDlg) progressDlg->close(); 
        QString errorString = tr("Can't add dropped file. ") + strtoqstr(e.getMessage());
        QMessageBox::warning(this, tr("Rosegarden"), errorString);
        return ;
    } catch (SoundFile::BadSoundFileException e) {
        CurrentProgressDialog::freeze();
        if (progressDlg) progressDlg->close(); 
        QString errorString = tr("Can't add dropped file. ") + strtoqstr(e.getMessage());
        QMessageBox::warning(this, tr("Rosegarden"), errorString);
        return;
    }

    if (progressDlg) progressDlg->close(); 
             
    progressDlg = new ProgressDialog(tr("Generating audio preview..."),
                               (QWidget*)this);

    connect(progressDlg, SIGNAL(canceled()),
            &aFM, SLOT(slotStopPreview()));
    
    try {
        aFM.generatePreview(audioFileId);
    } catch (Exception e) {
        CurrentProgressDialog::freeze();
        if (progressDlg) progressDlg->close(); 
        QString message = strtoqstr(e.getMessage()) + "\n\n" +
                          tr("Try copying this file to a directory where you have write permission and re-add it");
        QMessageBox::information(this, tr("Rosegarden"), message);
        return;
    }

    if (progressDlg) {
        disconnect(progressDlg, SIGNAL(canceled()),
                   &aFM, SLOT(slotStopPreview()));
    }

    // add the file at the sequencer
    emit addAudioFile(audioFileId);

    // Now fetch file details
    //
    AudioFile *aF = aFM.getAudioFile(audioFileId);

    if (aF) {
        
        RG_DEBUG << "RosegardenMainViewWidget::slotDroppedNewAudio("
        << "file = " << url
        << ", trackid = " << trackId
        << ", time = " << time << endl;
        
        slotAddAudioSegment(audioFileId, trackId, time,
                            RealTime(0, 0), aF->getLength());
        
    }
    
    if (progressDlg) progressDlg->close();  // note: Qt::WA_DeleteOnClose set
    CurrentProgressDialog::set(0);
}

void
RosegardenMainViewWidget::slotDroppedAudio(QString audioDesc)
{
    QTextStream s(&audioDesc, QIODevice::ReadOnly);

    AudioFileId audioFileId;
    TrackId trackId;
    timeT position;
    RealTime startTime, endTime;

    // read the audio info
    s >> audioFileId;
    s >> trackId;
    s >> position;
    s >> startTime.sec;
    s >> startTime.nsec;
    s >> endTime.sec;
    s >> endTime.nsec;

    RG_DEBUG << "RosegardenMainViewWidget::slotDroppedAudio("
    //<< audioDesc
    << ") : audioFileId = " << audioFileId
    << " - trackId = " << trackId
    << " - position = " << position
    << " - startTime.sec = " << startTime.sec
    << " - startTime.nsec = " << startTime.nsec
    << " - endTime.sec = " << endTime.sec
    << " - endTime.nsec = " << endTime.nsec
    << endl;

    slotAddAudioSegment(audioFileId, trackId, position, startTime, endTime);
}

void
RosegardenMainViewWidget::slotSetRecord(InstrumentId id, bool value)
{
    RG_DEBUG << "RosegardenMainViewWidget::slotSetRecord - "
    << "id = " << id
    << ",value = " << value << endl;
    /*
        // IPB
        //
        m_instrumentParameterBox->setRecord(value);
    */
#ifdef NOT_DEFINED
    Composition &comp = getDocument()->getComposition();
    Composition::trackcontainer &tracks = comp.getTracks();
    Composition::trackiterator it;

    for (it = tracks.begin(); it != tracks.end(); ++it) {
        if (comp.getSelectedTrack() == (*it).second->getId()) {
            //!!! MTR            m_trackEditor->getTrackButtons()->
            //                setRecordTrack((*it).second->getPosition());
            //!!! MTR is this needed? I think probably not
            slotUpdateInstrumentParameterBox((*it).second->getInstrument());
        }
    }
#endif
    // Studio &studio = getDocument()->getStudio();
    // Instrument *instr = studio.getInstrumentById(id);
}

void
RosegardenMainViewWidget::slotSetSolo(InstrumentId id, bool value)
{
    RG_DEBUG << "RosegardenMainViewWidget::slotSetSolo - "
    << "id = " << id
    << ",value = " << value << endl;

    emit toggleSolo(value);
}

void
RosegardenMainViewWidget::slotUpdateRecordingSegment(Segment *segment,
        timeT )
{
    // We're only interested in this on the first call per recording segment,
    // when we possibly create a view for it
    static Segment *lastRecordingSegment = 0;

    if (segment == lastRecordingSegment)
        return ;
    lastRecordingSegment = segment;

    QSettings settings;
    settings.beginGroup( GeneralOptionsConfigGroup );

    int tracking = settings.value("recordtracking", 0).toUInt() ;
    settings.endGroup();

    if (tracking != 1)
        return ;

    RG_DEBUG << "RosegardenMainViewWidget::slotUpdateRecordingSegment: segment is " << segment << ", lastRecordingSegment is " << lastRecordingSegment << ", opening a new view" << endl;

    std::vector<Segment *> segments;
    segments.push_back(segment);

    NotationView *view = createNotationView(segments);
    if (!view) return ;

    /* signal no longer exists
        QObject::connect
    	(getDocument(), SIGNAL(recordingSegmentUpdated(Segment *, timeT)),
    	 view, SLOT(slotUpdateRecordingSegment(Segment *, timeT)));
    */

    view->show();
}

void
RosegardenMainViewWidget::slotSynchroniseWithComposition()
{
    // Track buttons
    //
    m_trackEditor->getTrackButtons()->slotSynchroniseWithComposition();

    // Update all IPBs
    //
    Composition &comp = getDocument()->getComposition();
    Track *track = comp.getTrackById(comp.getSelectedTrack());
    slotUpdateInstrumentParameterBox(track->getInstrument());

    // Update the MatrixWidget's PitchRuler.
    m_instrumentParameterBox->emitInstrumentPercussionSetChanged();
}

void
RosegardenMainViewWidget::windowActivationChange(bool)
{
    if (isActiveWindow()) {
        slotActiveMainWindowChanged(this);
    }
}

void
RosegardenMainViewWidget::slotActiveMainWindowChanged(const QWidget *w)
{
    m_lastActiveMainWindow = w;
}

void
RosegardenMainViewWidget::slotActiveMainWindowChanged()
{
    const QWidget *w = dynamic_cast<const QWidget *>(sender());
    if (w)
        slotActiveMainWindowChanged(w);
}

void
RosegardenMainViewWidget::slotControllerDeviceEventReceived(MappedEvent *e)
{
    RG_DEBUG << "Controller device event received - send to " << (void *)m_lastActiveMainWindow << " (I am " << this << ")" << endl;

    //!!! So, what _should_ we do with these?

    // -- external controller that sends e.g. volume control for each
    // of a number of channels -> if mixer present, use control to adjust
    // tracks on mixer

    // -- external controller that sends e.g. separate controllers on
    // the same channel for adjusting various parameters -> if IPB
    // visible, adjust it.  Should we use the channel to select the
    // track? maybe as an option

    // do we actually need the last active main window for either of
    // these? -- yes, to determine whether to send to mixer or to IPB
    // in the first place.  Send to audio mixer if active, midi mixer
    // if active, plugin dialog if active, otherwise keep it for
    // ourselves for the IPB.  But, we'll do that by having the edit
    // views pass it back to us.

    // -- then we need to send back out to device.

    //!!! special cases: controller 81 received by any window ->
    // select window 0->main, 1->audio mix, 2->midi mix

    //!!! controller 82 received by main window -> select track

    //!!! these obviously should be configurable

    if (e->getType() == MappedEvent::MidiController) {

        if (e->getData1() == 81) {

            // select window
            int window = e->getData2();

            if (window < 10) { // me

                show();
                raise();
                activateWindow();

            } else if (window < 20) {

                RosegardenMainWindow::self()->slotOpenAudioMixer();

            } else if (window < 30) {

                RosegardenMainWindow::self()->slotOpenMidiMixer();
            }
        }
    }

    // Delegate to the currently active track-related window.
    // The idea here is that the user can remotely switch between the
    // three track-related windows (rg main, MIDI Mixer, and Audio Mixer)
    // via controller 81 (above).  Then they can control the tracks
    // on that window via other controllers.

    // The behavior is slightly different between the three windows.
    // The main window uses controller 82 to select a track while
    // the other two use the incoming MIDI channel to select which
    // track will be modified.
    emit controllerDeviceEventReceived(e, m_lastActiveMainWindow);
}

void
RosegardenMainViewWidget::slotControllerDeviceEventReceived(MappedEvent *e, const void *preferredCustomer)
{
    if (preferredCustomer != this)
        return ;
    RG_DEBUG << "RosegardenMainViewWidget::slotControllerDeviceEventReceived: this one's for me" << endl;
    raise();

    RG_DEBUG << "Event is type: " << int(e->getType()) << ", channel " << int(e->getRecordedChannel()) << ", data1 " << int(e->getData1()) << ", data2 " << int(e->getData2()) << endl;

    Composition &comp = getDocument()->getComposition();
    Studio &studio = getDocument()->getStudio();

    TrackId currentTrackId = comp.getSelectedTrack();
    Track *track = comp.getTrackById(currentTrackId);

    // If the event is a control change on channel n, then (if
    // follow-channel is on) switch to the nth track of the same type
    // as the current track -- or the first track of the given
    // channel?, and set the control appropriately.  Any controls in
    // IPB are supported for a MIDI device plus program and bank; only
    // volume and pan are supported for audio/synth devices.
    //!!! complete this

    if (e->getType() != MappedEvent::MidiController) {

        if (e->getType() == MappedEvent::MidiProgramChange) {
            int program = e->getData1();
            if (!track)
                return ;
            InstrumentId ii = track->getInstrument();
            Instrument *instrument = studio.getInstrumentById(ii);
            if (!instrument)
                return ;
            instrument->setProgramChange(program);
            instrument->changed();
        }
        return ;
    }

    // unsigned int channel = e->getRecordedChannel();
    MidiByte controller = e->getData1();
    MidiByte value = e->getData2();

    if (controller == 82) { // !!! magic select-track controller
        int tracks = comp.getNbTracks();
        Track *track = comp.getTrackByPosition(value * tracks / 128);
        if (track) {
            comp.setSelectedTrack(track->getId());
            comp.notifyTrackSelectionChanged(track->getId());

            slotSelectTrackSegments(track->getId());
        }
        return ;
    }

    if (!track)
        return ;

    InstrumentId ii = track->getInstrument();
    Instrument *instrument = studio.getInstrumentById(ii);

    if (!instrument)
        return ;

    switch (instrument->getType()) {

    case Instrument::Midi: {
            MidiDevice *md = dynamic_cast<MidiDevice *>(instrument->getDevice());
            if (!md) {
                std::cerr << "WARNING: MIDI instrument has no MIDI device in slotControllerDeviceEventReceived" << std::endl;
                return ;
            }

            ControlList cl = md->getControlParameters();
            for (ControlList::const_iterator i = cl.begin(); i != cl.end(); ++i) {
                if ((*i).getControllerValue() == controller) {
                    RG_DEBUG << "Setting controller " << controller << " for instrument " << instrument->getId() << " to " << value << endl;
                    instrument->setControllerValue(controller, value);
                    instrument->changed();
                    break;
                }
            }
            break;
        }

    case Instrument::SoftSynth:
    case Instrument::Audio:

        switch (controller) {

        case MIDI_CONTROLLER_VOLUME:
            RG_DEBUG << "Setting volume for instrument " << instrument->getId() << " to " << value << endl;
            instrument->setLevel(AudioLevel::fader_to_dB
                                 (value, 127, AudioLevel::ShortFader));
            instrument->changed();
            break;

        case MIDI_CONTROLLER_PAN:
            RG_DEBUG << "Setting pan for instrument " << instrument->getId() << " to " << value << endl;
            instrument->setControllerValue(MIDI_CONTROLLER_PAN, MidiByte((value / 64.0) * 100.0 + 0.01));
            instrument->changed();
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }

    //!!! send out updates via MIDI
}

void
RosegardenMainViewWidget::initChordNameRuler()
{
    getTrackEditor()->getChordNameRuler()->setReady();
}

EventView *
RosegardenMainViewWidget::createEventView(std::vector<Segment *> segmentsToEdit)
{
    EventView *eventView = new EventView(getDocument(),
                                         segmentsToEdit,
                                         this);

    connect(eventView, SIGNAL(windowActivated()),
        this, SLOT(slotActiveMainWindowChanged()));

    connect(eventView, SIGNAL(selectTrack(int)),
            this, SLOT(slotSelectTrackSegments(int)));

    connect(eventView, SIGNAL(saveFile()),
        RosegardenMainWindow::self(), SLOT(slotFileSave()));

    connect(eventView, SIGNAL(openInNotation(std::vector<Segment *>)),
        this, SLOT(slotEditSegmentsNotation(std::vector<Segment *>)));
    connect(eventView, SIGNAL(openInMatrix(std::vector<Segment *>)),
        this, SLOT(slotEditSegmentsMatrix(std::vector<Segment *>)));
    connect(eventView, SIGNAL(openInPercussionMatrix(std::vector<Segment *>)),
        this, SLOT(slotEditSegmentsPercussionMatrix(std::vector<Segment *>)));
    connect(eventView, SIGNAL(openInEventList(std::vector<Segment *>)),
        this, SLOT(slotEditSegmentsEventList(std::vector<Segment *>)));
    connect(eventView, SIGNAL(editTriggerSegment(int)),
        this, SLOT(slotEditTriggerSegment(int)));
    connect(this, SIGNAL(compositionStateUpdate()),
        eventView, SLOT(slotCompositionStateUpdate()));
    connect(RosegardenMainWindow::self(), SIGNAL(compositionStateUpdate()),
        eventView, SLOT(slotCompositionStateUpdate()));
    connect(eventView, SIGNAL(toggleSolo(bool)),
            RosegardenMainWindow::self(), SLOT(slotToggleSolo(bool)));

    // create keyboard shortcuts on view
    //
    RosegardenMainWindow *par = dynamic_cast<RosegardenMainWindow*>(parent());

    if (par) {
        par->plugShortcuts(eventView, eventView->getShortcuts());
    }

    return eventView;
}

bool RosegardenMainViewWidget::hasNonAudioSegment(const SegmentSelection &segments)
{
    for (SegmentSelection::const_iterator i = segments.begin();
         i != segments.end();
         ++i) {
        if ((*i)->getType() == Segment::Internal)
            return true;
    }
    return false;
}


}
#include "RosegardenMainViewWidget.moc"
