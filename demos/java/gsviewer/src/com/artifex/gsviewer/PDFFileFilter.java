package com.artifex.gsviewer;

import java.io.File;

import javax.swing.filechooser.FileFilter;

public class PDFFileFilter extends FileFilter {

	public static final PDFFileFilter INSTANCE = new PDFFileFilter();

	@Override
	public boolean accept(File f) {
		if (f.isDirectory())
			return true;
		String filename = f.getName();
		int ind = filename.lastIndexOf('.');
		if (ind == -1)
			return false;
		String ext = filename.substring(ind);
		return ext.equalsIgnoreCase(".pdf");
	}

	@Override
	public String getDescription() {
		return "PDF Files";
	}

}
