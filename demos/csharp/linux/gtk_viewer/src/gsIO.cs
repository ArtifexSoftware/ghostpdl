using System;
using System.ComponentModel;

namespace gs_mono_example
{
	class gsIO : INotifyPropertyChanged
	{
			public String gsIOString
			{
				get;
				set;
			}

			public event PropertyChangedEventHandler PropertyChanged;

			public void PageRefresh()
			{
				if (PropertyChanged != null)
				{
					PropertyChanged(this, new PropertyChangedEventArgs("gsIOString"));
				}
			}

			public gsIO()
			{
				this.gsIOString = "";
			}
	}
}
