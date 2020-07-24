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

				for (int i = 1; i <= doc.size(); i++) {
					doc.loadHighRes(i);
					source.setLoadProgress((int)((double)i / doc.size() * 100));
				}
				source.setLoadProgress(0);
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

	}

	@Override
	public void onZoomChange(double oldZoom, double newZoom) {

	}

	@Override
	public void onScrollChange(Point oldScroll, Point newScroll) {

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
