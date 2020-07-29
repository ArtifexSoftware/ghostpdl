package com.artifex.gsviewer;

import java.awt.Point;
import java.io.File;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import javax.swing.JFileChooser;
import javax.swing.JOptionPane;

import com.artifex.gsviewer.gui.ViewerGUIListener;
import com.artifex.gsviewer.gui.ViewerWindow;

public class ViewerController implements ViewerGUIListener {

	private final static Lock lock = new ReentrantLock();
	private final static Condition cv = lock.newCondition();

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

				dispatchSmartLoader();
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
		lock.lock();
		try {
			cv.signalAll();
		} catch (IllegalMonitorStateException e) {
			System.err.println("Exception on signaling: " + e);
		} finally {
			lock.unlock();
		}
	}

	@Override
	public void onZoomChange(double oldZoom, double newZoom) {

	}

	@Override
	public void onScrollChange(int newScroll) {
		lock.lock();
		try {
			cv.signalAll();
		} catch (IllegalMonitorStateException e) {
			System.err.println("Exception on signaling: " + e);
		} finally {
			lock.unlock();
		}
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

	private void dispatchSmartLoader() {
		Runnable r = () -> {
			boolean[] loaded = new boolean[currentDocument.size()];
			Document doc;
			while ((doc = source.getLoadedDocument()) != null) {
				int currentPage = source.getCurrentPage();
				int[] toLoad =  new int[] {
						currentPage, currentPage - 1, currentPage + 1,
						currentPage - 2, currentPage + 2 };

				int ind = 0;
				for (int page : toLoad) {
					if (page >= 1 && page <= doc.size()) {
						if (!loaded[page - 1]) {
							doc.loadHighRes(page);
							loaded[page - 1] = true;
						}
					}
					ind++;
					source.setLoadProgress((int)(((double)ind / toLoad.length) * 100));
				}
				source.setLoadProgress(0);

				lock.lock();
				try {
					cv.await();
				} catch (InterruptedException e) {
					System.err.println("Interrupted in smart loader: " + e);
				} catch (IllegalMonitorStateException e) {
					System.err.println("Exception in smart loader await: " + e);
				} finally {
					lock.unlock();
				}
			}
		};
		Thread t = new Thread(r);
		t.setDaemon(true);
		t.setName("Document-Smart-Loader-Thread");
		t.start();
	}
}
