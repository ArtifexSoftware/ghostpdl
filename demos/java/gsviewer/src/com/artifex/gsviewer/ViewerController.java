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
					ret = chooser.showSaveDialog(source);
					if (ret != JFileChooser.APPROVE_OPTION)
						return;
					File out = chooser.getSelectedFile();
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
		private volatile boolean shouldRun;
		private Thread thread;

		private SmartLoader(Document doc) {
			loaded = new boolean[doc.size()];
			shouldRun = true;
		}

		private void start() {
			if (thread != null)
				stop();
			shouldRun = true;
			thread = new Thread(this);
			thread.setDaemon(true);
			thread.setName("Document-Smart-Loader-Thread");
			thread.start();
		}

		private void stop() {
			shouldRun = false;
			lock.lock();
			cv.signalAll();
			lock.unlock();
			try {
				thread.join();
			} catch (InterruptedException e) {
				throw new RuntimeException("Thread join interrupted", e);
			}
			thread = null;
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
						if (!loaded[page - 1]) {
							currentDocument.loadHighRes(Document.OPERATION_WAIT, page);
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
				} finally {
					lock.unlock();
				}
			}
		}
	}
}
