/* This file is part of Ingen.
 * Copyright (C) 2007 Dave Robillard <http://drobilla.net>
 * 
 * Ingen is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Ingen is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "MidiTriggerNode.h"
#include <cmath>
#include "InputPort.h"
#include "OutputPort.h"
#include "Plugin.h"
#include "util.h"
#include "midi.h"

namespace Ingen {


MidiTriggerNode::MidiTriggerNode(const string& path, size_t poly, Patch* parent, SampleRate srate, size_t buffer_size)
: InternalNode(new Plugin(Plugin::Internal, "ingen:trigger_node"), path, 1, parent, srate, buffer_size)
{
	_ports = new Raul::Array<Port*>(5);

	_midi_in_port = new InputPort<MidiMessage>(this, "MIDI_In", 0, 1, DataType::MIDI, _buffer_size);
	_ports->at(0) = _midi_in_port;
	
	_note_port = new InputPort<Sample>(this, "Note_Number", 1, 1, DataType::FLOAT, 1);
	//	new PortInfo("Note Number", CONTROL, INPUT, INTEGER, 60, 0, 127), 1);
	_ports->at(1) = _note_port;
	
	_gate_port = new OutputPort<Sample>(this, "Gate", 2, 1, DataType::FLOAT, _buffer_size);
	//	new PortInfo("Gate", AUDIO, OUTPUT, 0, 0, 1), _buffer_size);
	_ports->at(2) = _gate_port;

	_trig_port = new OutputPort<Sample>(this, "Trigger", 3, 1, DataType::FLOAT, _buffer_size);
	//	new PortInfo("Trigger", AUDIO, OUTPUT, 0, 0, 1), _buffer_size);
	_ports->at(3) = _trig_port;
	
	_vel_port = new OutputPort<Sample>(this, "Velocity", 4, poly, DataType::FLOAT, _buffer_size);
	//	new PortInfo("Velocity", AUDIO, OUTPUT, 0, 0, 1), _buffer_size);
	_ports->at(4) = _vel_port;
	
	plugin()->plug_label("trigger_in");
	assert(plugin()->uri() == "ingen:trigger_node");
	plugin()->name("Ingen Trigger Node (MIDI, OSC)");
}


void
MidiTriggerNode::process(SampleCount nframes, FrameTime start, FrameTime end)
{
	InternalNode::process(nframes, start, end);
	
	MidiMessage ev;
	
	for (size_t i=0; i < _midi_in_port->buffer(0)->filled_size(); ++i) {
		ev = _midi_in_port->buffer(0)->value_at(i);

		switch (ev.buffer[0] & 0xF0) {
		case MIDI_CMD_NOTE_ON:
			if (ev.buffer[2] == 0)
				note_off(ev.buffer[1], ev.time, nframes, start, end);
			else
				note_on(ev.buffer[1], ev.buffer[2], ev.time, nframes, start, end);
			break;
		case MIDI_CMD_NOTE_OFF:
			note_off(ev.buffer[1], ev.time, nframes, start, end);
			break;
		case MIDI_CMD_CONTROL:
			if (ev.buffer[1] == MIDI_CTL_ALL_NOTES_OFF
					|| ev.buffer[1] == MIDI_CTL_ALL_SOUNDS_OFF)
				_gate_port->buffer(0)->set(0.0f, ev.time);
		default:
			break;
		}
	}
}


void
MidiTriggerNode::note_on(uchar note_num, uchar velocity, FrameTime time, SampleCount nframes, FrameTime start, FrameTime end)
{
	assert(time >= start && time <= end);
	assert(time - start < _buffer_size);

	//std::cerr << "Note on starting at sample " << offset << std::endl;

	const Sample filter_note = _note_port->buffer(0)->value_at(0);
	if (filter_note >= 0.0 && filter_note < 127.0 && (note_num == (uchar)filter_note)){
			
		// FIXME FIXME FIXME
		SampleCount offset = time - start;

		// See comments in MidiNoteNode::note_on (FIXME)
		if (offset == (SampleCount)(_buffer_size-1))
			--offset;
		
		_gate_port->buffer(0)->set(1.0f, offset);
		_trig_port->buffer(0)->set(1.0f, offset, offset);
		_trig_port->buffer(0)->set(0.0f, offset+1);
		_vel_port->buffer(0)->set(velocity/127.0f, offset);
	}
}


void
MidiTriggerNode::note_off(uchar note_num, FrameTime time, SampleCount nframes, FrameTime start, FrameTime end)
{
	assert(time >= start && time <= end);
	assert(time - start < _buffer_size);

	if (note_num == lrintf(_note_port->buffer(0)->value_at(0)))
		_gate_port->buffer(0)->set(0.0f, time - start);
}


} // namespace Ingen

