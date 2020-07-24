package com.artifex.gsviewer;

import java.awt.Point;
import java.io.File;

import javax.swing.JFileChooser;
import javax.swing.JOptionPane;

import com.artifex.gsviewer.gui.ViewerGUIListener;
import com.artifex.gsviewer.gui.ViewerWindow;

public class ViewerController implements ViewerGUIListener {

	private ViewerWindow source;
	private Document currentDocument;

	public void open(final File file) {
		if (currentDocument != null)
			close();
		Document.loadDocumentAsync(file, (final Document doc, final Exception exception) -> {
			source.setLoadProgress(0);
			if (exception != null) {
				JOptionPane.showMessageDialog(source, exception.toString(),
						"Failed to load", JOptionPane.ERROR_MESSAGE);
			} else {
				System.out.println("Loaded document");
				this.currentDocument = doc;
				source.loadDocumentToViewer(doc);
			}
		}, (int progress) -> {
			source.setLoadProgress(progress);
		});
	}

	public void close() {
		if (currentDocument != null) {
			source.loadDocumentToViewer(null);
			currentDocument.unload();
			currentDocument = null;
		}
	}

	public Document getCurrentDocument() {
		return currentDocument;
	}

	@Override
	public void onViewerAdd(ViewerWindow source) {
		this.source = source;
	}

	@Override
	public void onPageChange(int oldPage, int newPage) {
		System.out.println("Page change: " + oldPage + " to " + newPage);
	}

	@Override
	public void onZoomChange(double oldZoom, double newZoom) {
		System.out.println("Zoom change: " + oldZoom + " to " + newZoom);
	}

	@Override
	public void onScrollChange(Point oldScroll, Point newScroll) {
		System.out.println("Scroll change: " + oldScroll + " to " + newScroll);
	}

	@Override
	public void onOpenFile() {
		JFileChooser chooser = new JFileChooser();
		chooser.setFileFilter(GSFileFilter.INSTANCE);
		chooser.setCurrentDirectory(new File("").getAbsoluteFile());
		int status = chooser.showOpenDialog(source);
		if (status == JFileChooser.APPROVE_OPTION)
			open(chooser.getSelectedFile());
	}

	@Override
	public void onCloseFile() {
		close();
	}

	@Override
	public void onClosing() {
		source.dispose();
		System.exit(0);
	}

	@Override
	public void onSettingsOpen() {
		System.out.println("Settings open");
	}

}
