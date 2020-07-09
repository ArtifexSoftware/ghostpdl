using System;
namespace gs_mono_example
{
    public class gsOutput : Gtk.Window
    {
        public gsOutput() : base(Gtk.WindowType.Popup)
        {
            Gtk.TextBuffer m_textBuffer = new Gtk.TextBuffer()
            Gtk.TextView m_textView = new Gtk.TextView()
            Gtk.VBox vBox = new Gtk.VBox();

            this.Add(vBox);


        }
    }
}
