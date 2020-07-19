using System;
using Gtk;

namespace gs_mono_example
{

    class MainClass
    {
        public static void Main(string[] args)
        {
            Application.Init();
            MainWindow win = new MainWindow();
            win.ShowAll();

            try
            {
                Application.Run();
            }
            catch(Exception except)
            {
                var mess = except.Message;
            }
        }
    }
}
