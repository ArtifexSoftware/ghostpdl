package com.artifex.gsviewer;

import java.io.File;

import javax.swing.filechooser.FileFilter;

public class GSFileFilter extends FileFilter {

	public static final GSFileFilter INSTANCE = new GSFileFilter();

	@Override
	public boolean accept(File f) {
		if (f.isDirectory())
			return true;
		final String name = f.getName();
		final int ind = name.lastIndexOf('.');
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
