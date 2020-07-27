package com.artifex.gsviewer;

import java.io.File;

import javax.swing.filechooser.FileFilter;

/**
 * <p>A file filter used for the file choose to only accept certain
 * file extensions.</p>
 *
 * <p>This file filter only accepts files of type ".pdf", ".eps", ".ps",
 * ".xps", ".oxps", and ".bin"</p>
 *
 * @author Ethan Vrhel
 *
 */
public class GSFileFilter extends FileFilter {

	/**
	 * An instance of the file filter.
	 */
	public static final GSFileFilter INSTANCE = new GSFileFilter();

	@Override
	public boolean accept(File f) {
		if (f.isDirectory())
			return true;
		final String name = f.getName();
		final int ind = name.lastIndexOf('.');
		if (ind < 0)
			return false;
		final String ext = name.substring(ind);
		return
			equals(ext, ".pdf") ||
			equals(ext, ".eps") ||
			equals(ext, ".ps") ||
			equals(ext, ".xps") ||
			equals(ext, ".oxps") ||
			equals(ext, ".bin");
	}

	@Override
	public String getDescription() {
		return "Ghostscript Files";
	}

	private boolean equals(String extension, String compare) {
		return extension.equalsIgnoreCase(compare);
	}
}
