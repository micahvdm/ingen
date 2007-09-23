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

#include <cassert>
#include <raul/Atom.hpp>
#include "interface/EngineInterface.hpp"
#include "client/PatchModel.hpp"
#include "client/NodeModel.hpp"
#include "App.hpp"
#include "NodeModule.hpp"
#include "PatchCanvas.hpp"
#include "Port.hpp"
#include "GladeFactory.hpp"
#include "RenameWindow.hpp"
#include "PatchWindow.hpp"
#include "WindowFactory.hpp"
#include "SubpatchModule.hpp"
#include "NodeControlWindow.hpp"

namespace Ingen {
namespace GUI {


NodeModule::NodeModule(boost::shared_ptr<PatchCanvas> canvas, SharedPtr<NodeModel> node)
	: FlowCanvas::Module(canvas, node->path().name())
	, _node(node)
	, _gui(NULL)
	, _gui_item(NULL)
{
	assert(_node);
	
	Glib::RefPtr<Gnome::Glade::Xml> xml = GladeFactory::new_glade_reference();
	xml->get_widget_derived("object_menu", _menu);
	_menu->init(node);
	set_menu(_menu);

	node->signal_new_port.connect(sigc::bind(sigc::mem_fun(this, &NodeModule::add_port), true));
	node->signal_removed_port.connect(sigc::mem_fun(this, &NodeModule::remove_port));
	node->signal_metadata.connect(sigc::mem_fun(this, &NodeModule::set_metadata));
	node->signal_polyphonic.connect(sigc::mem_fun(this, &NodeModule::set_stacked_border));
	node->signal_renamed.connect(sigc::mem_fun(this, &NodeModule::rename));

	_menu->signal_embed_gui.connect(sigc::mem_fun(this, &NodeModule::embed_gui));
	
	set_stacked_border(node->polyphonic());
}


NodeModule::~NodeModule()
{
	NodeControlWindow* win = App::instance().window_factory()->control_window(_node);
	
	if (win) {
		// Should remove from window factory via signal
		delete win;
	}
}


boost::shared_ptr<NodeModule>
NodeModule::create(boost::shared_ptr<PatchCanvas> canvas, SharedPtr<NodeModel> node)
{
	boost::shared_ptr<NodeModule> ret;

	SharedPtr<PatchModel> patch = PtrCast<PatchModel>(node);
	if (patch)
		ret = boost::shared_ptr<NodeModule>(new SubpatchModule(canvas, patch));
	else
		ret = boost::shared_ptr<NodeModule>(new NodeModule(canvas, node));

	for (MetadataMap::const_iterator m = node->metadata().begin(); m != node->metadata().end(); ++m)
		ret->set_metadata(m->first, m->second);

	for (PortModelList::const_iterator p = node->ports().begin(); p != node->ports().end(); ++p)
		ret->add_port(*p, false);

	ret->resize();

	return ret;
}


void
NodeModule::embed_gui(bool embed)
{
	if (embed) {
				
		GtkWidget* c_widget = NULL;

		if (!_gui_item) {
			cerr << "Embedding LV2 GUI" << endl;
			// FIXME: leaks?
			SLV2UIInstance ui = _node->plugin()->ui(App::instance().engine().get(), _node.get());
			if (ui) {
				cerr << "Found UI" << endl;
				c_widget = (GtkWidget*)slv2_ui_instance_get_widget(ui);
				_gui = Glib::wrap(c_widget);
				assert(_gui);
				const double y = 4 + _canvas_title.property_text_height();
				_gui_item = new Gnome::Canvas::Widget(/**_canvas.lock()->root()*/*this, 2.0, y, *_gui);
			}
		}

		if (_gui_item) {
			assert(_gui);
			cerr << "Created canvas item" << endl;
			_gui->show();
			_gui->show_all();
			_gui_item->show();
			GtkRequisition r;
			gtk_widget_size_request(c_widget, &r);
			cerr << "Size request: " << r.width << "x" << r.height << endl;
			_width = max(_width, (double)r.width);
			_height = max(_height, (double)r.height);
			_gui_item->property_width() = _width - 2;
			_gui_item->property_height() = _height;
			_gui_item->raise_to_top();
			_ports_y_offset = _height + 2;
			set_width(_width);
		} else {
			cerr << "*** Failed to create canvas item" << endl;
		}

	} else {
		if (_gui_item)
			_gui_item->hide();

		_ports_y_offset = 0;
	}

	resize();
}


void
NodeModule::rename()
{
	set_name(_node->path().name());
}


void
NodeModule::add_port(SharedPtr<PortModel> port, bool resize_to_fit)
{
	Module::add_port(boost::shared_ptr<Port>(new Port(
			PtrCast<NodeModule>(shared_from_this()), port)));

	if (resize_to_fit)
		resize();
}


void
NodeModule::remove_port(SharedPtr<PortModel> port)
{
	SharedPtr<FlowCanvas::Port> p = Module::remove_port(port->path().name());
	p.reset();
}


void
NodeModule::show_control_window()
{
#ifdef HAVE_SLV2
	if (_node->plugin()->type() == PluginModel::LV2) {
		// FIXME: check type

		SLV2UIInstance ui = _node->plugin()->ui(App::instance().engine().get(), _node.get());
		if (ui) {
			cerr << "Showing LV2 GUI" << endl;
			// FIXME: leak
			GtkWidget* c_widget = (GtkWidget*)slv2_ui_instance_get_widget(ui);
			Gtk::Widget* widget = Glib::wrap(c_widget);
			
			Gtk::Window* win = new Gtk::Window();
			win->add(*widget);
			widget->show_all();
			win->show_all();
			win->present();
			widget->show_all();
			win->show_all();
		} else {
			cerr << "No LV2 GUI, showing builtin controls" << endl;
			App::instance().window_factory()->present_controls(_node);
		}
	} else {
		App::instance().window_factory()->present_controls(_node);
	}
#else
	App::instance().window_factory()->present_controls(_node);
#endif
}


void
NodeModule::store_location()
{
	const float x = static_cast<float>(property_x());
	const float y = static_cast<float>(property_y());
	
	const Atom& existing_x = _node->get_metadata("ingenuity:canvas-x");
	const Atom& existing_y = _node->get_metadata("ingenuity:canvas-y");
	
	if (existing_x.type() != Atom::FLOAT || existing_y.type() != Atom::FLOAT
			|| existing_x.get_float() != x || existing_y.get_float() != y) {
		App::instance().engine()->set_metadata(_node->path(), "ingenuity:canvas-x", Atom(x));
		App::instance().engine()->set_metadata(_node->path(), "ingenuity:canvas-y", Atom(y));
	}
}


void
NodeModule::set_metadata(const string& key, const Atom& value)
{
	if (key == "ingenuity:canvas-x" && value.type() == Atom::FLOAT)
		move_to(value.get_float(), property_y());
	else if (key == "ingenuity:canvas-y" && value.type() == Atom::FLOAT)
		move_to(property_x(), value.get_float());
}


} // namespace GUI
} // namespace Ingen
