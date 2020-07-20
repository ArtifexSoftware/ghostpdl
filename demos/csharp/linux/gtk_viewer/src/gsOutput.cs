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
