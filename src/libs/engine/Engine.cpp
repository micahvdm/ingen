/* This file is part of Ingen.  Copyright (C) 2006 Dave Robillard.
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

#include <cassert>
#include "Engine.h"	
#include "config.h"
#include "tuning.h"
#include <sys/mman.h>
#include <iostream>
#include <unistd.h>
#include "raul/Queue.h"
#include "Event.h"
#include "JackAudioDriver.h"
#include "NodeFactory.h"
#include "ClientBroadcaster.h"
#include "Patch.h"
#include "ObjectStore.h"
#include "MaidObject.h"
#include "Maid.h"
#include "MidiDriver.h"
#include "QueuedEventSource.h"
#include "PostProcessor.h"
#include "CreatePatchEvent.h"
#include "EnablePatchEvent.h"
#ifdef HAVE_JACK_MIDI
#include "JackMidiDriver.h"
#endif
#ifdef HAVE_ALSA_MIDI
#include "AlsaMidiDriver.h"
#endif
#ifdef HAVE_LASH
#include "LashDriver.h"
#endif
using std::cout; using std::cerr; using std::endl;

namespace Ingen {


Engine::Engine()
: m_midi_driver(NULL),
  m_maid(new Maid(maid_queue_size)),
  m_post_processor(new PostProcessor(*m_maid, post_processor_queue_size)),
  m_broadcaster(new ClientBroadcaster()),
  m_object_store(new ObjectStore()),
  m_node_factory(new NodeFactory()),
#ifdef HAVE_LASH
  m_lash_driver(new LashDriver()),
#else 
  m_lash_driver(NULL),
#endif
  m_quit_flag(false),
  m_activated(false)
{
}


Engine::~Engine()
{
	deactivate();

	for (Tree<GraphObject*>::iterator i = m_object_store->objects().begin();
			i != m_object_store->objects().end(); ++i) {
		if ((*i)->parent() == NULL)
			delete (*i);
	}
	
	delete m_object_store;
	delete m_broadcaster;
	delete m_node_factory;
	delete m_midi_driver;
	
	delete m_maid;

	munlockall();
}


/* driver() template specializations.
 * Due to the lack of RTTI, this needs to be implemented manually like this.
 * If more types/drivers start getting added, it may be worth it to enable
 * RTTI and put all the drivers into a map with typeid's as the key.  That's
 * more elegant and extensible, but this is faster and simpler - for now.
 */
template<>
Driver<MidiMessage>* Engine::driver<MidiMessage>() { return m_midi_driver; }
template<>
Driver<Sample>* Engine::driver<Sample>() { return m_audio_driver.get(); }


int
Engine::main()
{
	// Loop until quit flag is set (by OSCReceiver)
	while ( ! m_quit_flag) {
		nanosleep(&main_rate, NULL);
		main_iteration();
	}
	cout << "[Main] Done main loop." << endl;
	
	if (m_activated)
		deactivate();

	sleep(1);
	cout << "[Main] Exiting..." << endl;
	
	return 0;
}


/** Run one iteration of the main loop.
 *
 * NOT realtime safe (this is where deletion actually occurs)
 */
bool
Engine::main_iteration()
{
#ifdef HAVE_LASH
	// Process any pending LASH events
	if (lash_driver->enabled())
		lash_driver->process_events();
#endif
	// Run the maid (garbage collector)
	m_maid->cleanup();
	
	return !m_quit_flag;
}


bool
Engine::activate(SharedPtr<AudioDriver> ad, SharedPtr<EventSource> es)
{
	if (m_activated)
		return false;

	// Setup drivers
	m_audio_driver = ad;
#ifdef HAVE_JACK_MIDI
	m_midi_driver = new JackMidiDriver(((JackAudioDriver*)m_audio_driver.get())->jack_client());
#elif HAVE_ALSA_MIDI
	m_midi_driver = new AlsaMidiDriver(m_audio_driver);
#else
	m_midi_driver = new DummyMidiDriver();
#endif
	
	// Set event source (FIXME: handle multiple sources)
	m_event_source = es;

	m_event_source->activate();

	// Create root patch

	Patch* root_patch = new Patch("", 1, NULL,
			m_audio_driver->sample_rate(), m_audio_driver->buffer_size(), 1);
	root_patch->activate();
	root_patch->add_to_store(m_object_store);
	root_patch->process_order(root_patch->build_process_order());
	root_patch->enable();

	assert(m_audio_driver->root_patch() == NULL);
	m_audio_driver->set_root_patch(root_patch);

	m_audio_driver->activate();
#ifdef HAVE_ALSA_MIDI
	m_midi_driver->activate();
#endif
	
	m_post_processor->start();

	m_activated = true;
	
	return true;
}


void
Engine::deactivate()
{
	if (!m_activated)
		return;
	
	m_audio_driver->root_patch()->disable();
	m_audio_driver->root_patch()->deactivate();

	/*for (Tree<GraphObject*>::iterator i = m_object_store->objects().begin();
			i != m_object_store->objects().end(); ++i)
		if ((*i)->as_node() != NULL && (*i)->as_node()->parent() == NULL)
			(*i)->as_node()->deactivate();*/
	
	if (m_midi_driver != NULL)
		m_midi_driver->deactivate();
	
	m_audio_driver->deactivate();

	// Finalize any lingering events (unlikely)
	m_post_processor->whip();
	m_post_processor->stop();

	m_audio_driver.reset();

	m_event_source.reset();
	
	m_activated = false;
}


} // namespace Ingen
