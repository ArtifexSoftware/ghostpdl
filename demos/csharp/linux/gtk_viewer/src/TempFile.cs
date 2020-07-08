using System;
using System.IO;

/* A class to help in the management of temp files */
namespace gs_mono_example
{
	class TempFile
	{
		private String m_filename;

		public TempFile(String Name)
		{
			m_filename = Name;
		}

		public String Filename
		{
			get { return m_filename; }
		}

		public void DeleteFile()
		{
			try
			{
				if (File.Exists(m_filename))
					File.Delete(m_filename);
			}
			catch (Exception)
			{
				throw;
			}
		}
	}
}
