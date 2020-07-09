using System;
using Gtk;

namespace gs_mono_example
{
    public class gsOutput : Gtk.Window
    {
        public Gtk.TextView m_textView;

        public gsOutput() : base(Gtk.WindowType.Toplevel)
        {
            this.SetDefaultSize(500, 500); 
            this.Title = "GhostPDL Standard Output";
            VBox vBox = new VBox(true, 0);
            this.Add(vBox);

            this.DeleteEvent += Handle_DeleteEvent;

            m_textView = new Gtk.TextView();
            vBox.PackStart(m_textView, true, true, 0);
            this.ShowAll();
            this.Hide();
        }

        /* Don't actually destroy this window */
        void Handle_DeleteEvent(object o, DeleteEventArgs args)
        {
            this.Hide();
            args.RetVal = true;
        }

    }
}
