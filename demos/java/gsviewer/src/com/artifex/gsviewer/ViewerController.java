package com.artifex.gsviewer;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.PrintStream;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import javax.swing.JFileChooser;
import com.artifex.gsviewer.gui.ViewerGUIListener;
import com.artifex.gsviewer.gui.ViewerWindow;

/**
 * The ViewerController is an implementation of a <code>ViewerGUIListener</code>
 * used to control the main logic of the viewer such as determining what pages
 * should be loaded.
 *
 * @author Ethan Vrhel
 *
 */
public class ViewerController implements ViewerGUIListener {

	private static final Lock lock = new ReentrantLock();
	private static final Condition cv = lock.newCondition();

	private ViewerWindow source;
	private Document currentDocument;

	public void open(final File file) {
		if (currentDocument != null)
			close();
		Document.loadDocumentAsync(file, (final Document doc, final Exception exception) -> {
			// Don't allow multiple ghostscript operations at once
				//source.showWarningDialog("Error", "An operation is already in progress");
			source.loadDocumentToViewer(doc);
			source.setLoadProgress(0);
			if (exception != null) {
				source.showErrorDialog("Failed to load", exception.toString());
			} else {
				this.currentDocument = doc;
				dispatchSmartLoader();
			}
		}, (int progress) -> {
			source.setLoadProgress(progress);
		}, Document.OPERATION_RETURN);
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
		Thread.setDefaultUncaughtExceptionHandler(new UnhandledExceptionHandler());
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
		if (newZoom > 1.0) {
			int currentPage = source.getCurrentPage();
			Runnable r = () -> {
					//source.showWarningDialog("Error", "An operation is already in progress");
					//return;

				try {
					currentDocument.zoomArea(Document.OPERATION_THROW, currentPage, newZoom);
				} catch (Document.OperationInProgressException e) {
					source.showWarningDialog("Error", "An operation is already in progress");
				}
			};
			Thread t = new Thread(r);
			t.setName("Zoom-Thread");
			t.start();
		}
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
	public void onSettingsOpen() { }

	private void dispatchSmartLoader() {
		Thread t = new Thread(new SmartLoader());
		t.setDaemon(true);
		t.setName("Document-Smart-Loader-Thread");
		t.start();
	}

	private class UnhandledExceptionHandler implements Thread.UncaughtExceptionHandler {

		@Override
		public void uncaughtException(Thread t, Throwable e) {
			if (source != null) {
				ByteArrayOutputStream os = new ByteArrayOutputStream();
				PrintStream out = new PrintStream(os);
				e.printStackTrace(out);
				String errorMessage = new String(os.toByteArray());
				source.showErrorDialog("Unhandled Exception", errorMessage);
			}
			DefaultUnhandledExceptionHandler.INSTANCE.uncaughtException(t, e);
		}

	}

	private class SmartLoader implements Runnable {

		@Override
		public void run() {
			System.out.println("Smart loader dispatched.");
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
							doc.loadHighRes(Document.OPERATION_WAIT, page);
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
		}
	}
}
