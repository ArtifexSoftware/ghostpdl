using System;
namespace gtk_viewer.src
{
    public partial class gsOutput : Gtk.Window
    {
        public gsOutput() :
                base(Gtk.WindowType.Toplevel)
        {
            this.Build();
        }
    }
}
