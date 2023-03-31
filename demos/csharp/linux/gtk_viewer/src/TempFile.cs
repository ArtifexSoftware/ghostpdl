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