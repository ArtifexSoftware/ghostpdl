/* Copyright (C) 2020-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/

using System;
using Gtk;

namespace gs_mono_example
{
    public class gsOutput : Gtk.Window
    {
        public Gtk.TextView m_textView;
        public Gtk.ScrolledWindow m_scrolledWindow;

        public gsOutput() : base(Gtk.WindowType.Toplevel)
        {
            this.SetDefaultSize(500, 500); 
            this.Title = "GhostPDL Standard Output";

            m_scrolledWindow = new ScrolledWindow();
            m_scrolledWindow.BorderWidth = 5;
            m_scrolledWindow.ShadowType = ShadowType.In;

            DeleteEvent += Handle_DeleteEvent;

            m_textView = new Gtk.TextView();
            m_textView.Editable = false;

            /* To force resize and scroll to bottom when text view is update */
            m_scrolledWindow.Add(m_textView);

            Add(m_scrolledWindow);
            ShowAll();
            Hide();
        }

        /* Don't actually destroy this window */
        void Handle_DeleteEvent(object o, DeleteEventArgs args)
        {
            this.Hide();
            args.RetVal = true;
        }

    }
}