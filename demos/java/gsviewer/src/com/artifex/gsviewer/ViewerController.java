package com.artifex.gsviewer;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
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
	private static volatile boolean outOfDate = true;

	private ViewerWindow source;
	private Document currentDocument;
	private SmartLoader smartLoader;

	public void open(File file) {
		if (Document.shouldDistill(file)) {
			int ret = source.showConfirmDialog("Distill", "Would you like to distill this document before opening?");
			if (ret == ViewerWindow.CANCEL)
				return;
			else if (ret == ViewerWindow.YES) {
				try {
					JFileChooser chooser = new JFileChooser();
					chooser.setCurrentDirectory(new File("."));
					chooser.setFileFilter(PDFFileFilter.INSTANCE);
					ret = chooser.showSaveDialog(source);
					if (ret != JFileChooser.APPROVE_OPTION)
						return;
					File out = chooser.getSelectedFile();
					String filepath = out.getAbsolutePath();
					if (filepath.lastIndexOf('.') == -1)
						out = new File(filepath + ".pdf");
					if (out.exists()) {
						ret = source.showConfirmDialog("Overwrite?", out.getName() + " already exists. Overwrite?");
						if (ret != ViewerWindow.YES)
							return;
					}
					if (currentDocument != null)
						close();
					file = Document.distillDocument(file, out);
				} catch (IllegalStateException | IOException e) {
					System.err.println("Failed to distill: " + e);
				}
			}
		}

		if (currentDocument != null)
			close();
		Document.loadDocumentAsync(file, (final Document doc, final Exception exception) -> {
			source.loadDocumentToViewer(doc);
			source.setLoadProgress(0);
			if (exception != null) {
				source.showErrorDialog("Failed to load", exception.toString());
			} else {
				this.currentDocument = doc;
				dispatchSmartLoader();
			}
			source.revalidate();
		}, (int progress) -> {
			source.setLoadProgress(progress);
		}, Document.OPERATION_RETURN);
	}

	public void close() {
		if (smartLoader != null) {
			smartLoader.stop();
			smartLoader = null;
		}

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
		smartLoader.signalOutOfDate();
	}

	@Override
	public void onZoomChange(double oldZoom, double newZoom) {
		if (newZoom > 1.0) {
			smartLoader.resetZoom();
			smartLoader.signalOutOfDate();
		}
	}

	@Override
	public void onScrollChange(int newScroll) {
		if (smartLoader != null)
			smartLoader.signalOutOfDate();
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
		if (smartLoader != null)
			smartLoader.stop();
		smartLoader = new SmartLoader(currentDocument);
		smartLoader.start();
	}

	private class UnhandledExceptionHandler implements Thread.UncaughtExceptionHandler {

		@Override
		public void uncaughtException(Thread t, Throwable e) {
			e.printStackTrace(System.err);
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

		private volatile boolean[] loaded;
		private volatile boolean[] zoomLoaded;
		private volatile boolean shouldRun;
		private Thread thread;

		SmartLoader(Document doc) {
			loaded = new boolean[doc.size()];
			zoomLoaded = new boolean[doc.size()];
			shouldRun = true;
		}

		void start() {
			if (thread != null)
				stop();
			shouldRun = true;
			thread = new Thread(this);
			thread.setDaemon(true);
			thread.setName("Document-Smart-Loader-Thread");
			thread.start();
		}

		void stop() {
			lock.lock();
			cv.signalAll();
			lock.unlock();
			outOfDate = false;
			shouldRun = false;
			try {
				thread.join();
			} catch (InterruptedException e) {
				throw new RuntimeException("Thread join interrupted", e);
			}
			thread = null;
		}

		void resetZoom() {
			for (int i = 0; i < zoomLoaded.length; i++) {
				zoomLoaded[i] = false;
			}
		}

		void signalOutOfDate() {
			outOfDate = true;
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
		public void run() {
			System.out.println("Smart loader dispatched.");
			while (shouldRun) {
				int currentPage = source.getCurrentPage();
				int[] toLoad =  new int[] {
						currentPage, currentPage - 1, currentPage + 1,
						currentPage - 2, currentPage + 2 };

				if (loaded.length != currentDocument.size())
					throw new IllegalStateException("Array is size " + loaded.length + " while doc size is " + currentDocument.size());

				int ind = 0;
				for (int page : toLoad) {
					if (page >= 1 && page <= currentDocument.size()) {
						if (source.getZoom() > 1.0) {

							// Load the zoomed page view only if it has not already been loaded.
							if (!zoomLoaded[page - 1]) {
								currentDocument.zoomArea(Document.OPERATION_WAIT, page, source.getZoom());
								zoomLoaded[page - 1] = true;
							}
						} else {

							// Unload any zoomed image to save memory consumption
							currentDocument.unloadZoomed(page);

							// Load the high-resolution page view only if it has not already been loaded
							if (!loaded[page - 1]) {
								currentDocument.loadHighRes(Document.OPERATION_WAIT, page);
								loaded[page - 1] = true;
							}
						}
					}
					ind++;
					source.setLoadProgress((int)(((double)ind / toLoad.length) * 100));
				}
				source.setLoadProgress(0);

				// First check if the current view is out of date and if so, just immediately continue
				// so the thread does not get stuck in the lock until another event has occurred.
				if (!outOfDate) {
					outOfDate = false;
					lock.lock();
					try {
						cv.await();
					} catch (InterruptedException e) {
						System.err.println("Interrupted in smart loader: " + e);
					} finally {
						lock.unlock();
					}
				} else {
					outOfDate = false;
				}
			}
		}
	}
}
