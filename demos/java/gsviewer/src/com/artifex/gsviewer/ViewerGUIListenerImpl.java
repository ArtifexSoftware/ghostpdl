package com.artifex.gsviewer;

import java.awt.Point;
import java.io.File;

import javax.swing.JFileChooser;
import javax.swing.JOptionPane;

import com.artifex.gsviewer.gui.ViewerGUIListener;
import com.artifex.gsviewer.gui.ViewerWindow;

public class ViewerGUIListenerImpl implements ViewerGUIListener {

	@Override
	public void onPageChange(ViewerWindow source, int oldPage, int newPage) {
		System.out.println("Page change: " + oldPage + " to " + newPage);
	}

	@Override
	public void onZoomChange(ViewerWindow source, double oldZoom, double newZoom) {
		System.out.println("Zoom change: " + oldZoom + " to " + newZoom);
	}

	@Override
	public void onScrollChange(ViewerWindow source, Point oldScroll, Point newScroll) {
		System.out.println("Scroll change: " + oldScroll + " to " + newScroll);
	}

	@Override
	public void onOpenFile(ViewerWindow source) {
		System.out.println("Open file");
		JFileChooser chooser = new JFileChooser();
		chooser.setFileFilter(GSFileFilter.INSTANCE);
		chooser.setCurrentDirectory(new File("").getAbsoluteFile());
		int status = chooser.showOpenDialog(source);
		if (status == JFileChooser.APPROVE_OPTION) {
			File toOpen = chooser.getSelectedFile();
			System.out.println("To open: " + toOpen.getAbsolutePath());
			Document.loadDocumentAsync(toOpen, (final Document doc, final Exception exception) -> {
				if (exception != null) {
					JOptionPane.showMessageDialog(source, exception.toString(),
							"Failed to load", JOptionPane.ERROR_MESSAGE);
				} else {
					System.out.println("Loaded document");
				}
			});
		}
	}

	@Override
	public void onCloseFile(ViewerWindow source) {
		System.out.println("Close file");
	}

	@Override
	public void onClosing(ViewerWindow source) {
		System.out.println("Closing");
		source.dispose();
		System.exit(0);
	}

	@Override
	public void onSettingsOpen(ViewerWindow source) {
		System.out.println("Settings open");
	}

}
