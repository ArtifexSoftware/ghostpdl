using System;
using System.Windows;

namespace ghostnet_wpf_example
{
	/// <summary>
	/// Interaction logic for gsOutput.xaml
	/// </summary>
	public partial class gsOutput : Window
	{
		gsIO m_gsIO;
		public gsOutput()
		{
			InitializeComponent();
			this.Closing += new System.ComponentModel.CancelEventHandler(FakeWindowClosing); 
			m_gsIO = new gsIO();
			xaml_gsText.DataContext = m_gsIO;
		}

		void FakeWindowClosing(object sender, System.ComponentModel.CancelEventArgs e)
		{
			e.Cancel = true;
			this.Hide();
		}

		private void HideWindow(object sender, RoutedEventArgs e)
		{
			this.Hide();
		}

		public void RealWindowClosing()
		{
			this.Closing -= new System.ComponentModel.CancelEventHandler(FakeWindowClosing);
			this.Close();
		}

		public void Update(String newstring, int len)
		{
			m_gsIO.gsIOString += newstring.Substring(0, len);
			m_gsIO.PageRefresh();
		}

		private void ClearContents(object sender, RoutedEventArgs e)
		{
			m_gsIO.gsIOString = null;
			m_gsIO.PageRefresh();
		}
	}
}
