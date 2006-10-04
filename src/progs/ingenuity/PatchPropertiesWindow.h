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

#ifndef PATCHPROPERTIESWINDOW_H
#define PATCHPROPERTIESWINDOW_H

#include <string>
#include <gtkmm.h>
#include <libglademm/xml.h>
#include "raul/SharedPtr.h"
using std::string;

namespace Ingen { namespace Client { class PatchModel; } }
using Ingen::Client::PatchModel;

namespace Ingenuity {
	

/** Patch Properties Window.
 *
 * Loaded by libglade as a derived object.
 *
 * \ingroup Ingenuity
 */
class PatchPropertiesWindow : public Gtk::Window
{
public:
	PatchPropertiesWindow(BaseObjectType* cobject, const Glib::RefPtr<Gnome::Glade::Xml>& refGlade);

	void present(SharedPtr<PatchModel> patch_model) { set_patch(patch_model); Gtk::Window::present(); }
	void set_patch(SharedPtr<PatchModel> patch_model);
	
	void cancel_clicked();
	void ok_clicked();

private:
	SharedPtr<PatchModel> m_patch_model;

	Gtk::Entry*    m_author_entry;
	Gtk::TextView* m_textview;
	Gtk::Button*   m_cancel_button;
	Gtk::Button*   m_ok_button;
};


} // namespace Ingenuity

#endif // PATCHPROPERTIESWINDOW_H
