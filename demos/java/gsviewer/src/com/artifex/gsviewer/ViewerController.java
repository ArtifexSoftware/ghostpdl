package com.artifex.gsviewer;

import java.awt.HeadlessException;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.PrintStream;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import javax.swing.JFileChooser;
import com.artifex.gsviewer.gui.SettingsDialog;
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

	/**
	 * The lock for the SmartLoader.
	 */
	private static final Lock lock = new ReentrantLock();

	/**
	 * The condition variable for the SmartLoader to signal when an operation
	 * needs to happen.
	 */
	private static final Condition cv = lock.newCondition();

	/**
	 * The flag for the smart loader to read after completing an operation. If
	 * this flag is <code>true</code>, the SmartLoader will skip waiting for
	 * <code>cv</code> to be signaled and will immediately continue to the next
	 * operation.
	 */
	private static volatile boolean outOfDate = true;

	private ViewerWindow source; // The viewer window where documents will be displayed
	private Document currentDocument; // The current document loaded in the viewer
	private SmartLoader smartLoader; // The SmartLoader used to load pages based on scroll and zoom
	private boolean loadingDocument;

	/**
	 * <p>Opens a document from a files asynchronously.</p>
	 *
	 * <p>This method handles distilling files if they should be distilled and the user
	 * chooses to distill them.</p>
	 *
	 * @param file The file to load.
	 * @throws NullPointerException When <code>file</code> is <code>null</code>.
	 * @throws IllegalArgumentException If <code>file</code> is not a file type which can be loaded
	 * into the viewer, if distilling occurs and fails.
	 * @throws FileNotFoundException When <code>file</code> does not exist.
	 * @throws IOException If distilling occurs and a temporary files to be created.
	 * @throws HeadlessException If distilling occurs and <code>GraphicsEnvironment.isHeadless()</code> returns
	 * <code>true</code>.
	 * @throws RuntimeException When an error occurs while launching the loader thread.
	 */
	public void open(File file)
			throws NullPointerException, IllegalArgumentException, FileNotFoundException, IOException,
			HeadlessException, RuntimeException {
		if (loadingDocument) {
			source.showWarningDialog("Error", "A document is already loading");
			return;
		}
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
		loadingDocument = true;
		Document.loadDocumentAsync(file, (final Document doc, final Exception exception) -> {
			try {
				source.loadDocumentToViewer(doc);
				source.setLoadProgress(0);
				if (exception != null) {
					throw new RuntimeException("Failed to load", exception);
					//source.showErrorDialog("Failed to load", exception.toString());
				} else {
					this.currentDocument = doc;
					dispatchSmartLoader();
				}
				source.revalidate();
			} finally {
				loadingDocument = false;
			}
		}, (int progress) -> {
			source.setLoadProgress(progress);
		}, Document.OPERATION_RETURN);
	}

	/**
	 * Closes the currently loaded document, if one is loaded. If no document
	 * is loaded, this method does nothing.
	 */
	public void close() {
		if (loadingDocument) {
			source.showWarningDialog("Error", "A document is already loading");
			return;
		}

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

	/**
	 * Returns the current document being viewed.
	 *
	 * @return The current document being viewed or <code>null</code> if
	 * no document is being viewed.
	 */
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
		if (status == JFileChooser.APPROVE_OPTION) {
			try {
				open(chooser.getSelectedFile());
			} catch (IOException | RuntimeException e) {
				source.showErrorDialog("Failed to Open", "Failed to open file (" + e + ")");
			}
		}
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
		SettingsDialog dialog = new SettingsDialog(source, true);
		dialog.setVisible(true);
	}

	/**
	 * Starts the SmartLoader on the currently loaded document. If a
	 * SmartLoader already exists, it will be stopped.
	 */
	private void dispatchSmartLoader() {
		if (smartLoader != null)
			smartLoader.stop();
		smartLoader = new SmartLoader(currentDocument);
		smartLoader.start();
	}

	/**
	 * Implementation of <code>Thread.UncaughtExceptionHandler</code> to handle unhandled
	 * exceptions. This is used to show an error dialog to the user describing the error
	 * and then routes the error to the instance of the <code>DefaultUnhandledExceptionHandler</code>.
	 *
	 * @author Ethan Vrhel
	 *
	 */
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
				int[] toLoad = genToLoad(currentPage);

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

		int[] genToLoad(int page) {
			int range = Settings.SETTINGS.getSetting("preloadRange");
			int[] arr = new int[range * 2 + 1];
			for (int i = -range; i <= range; i++) {
				arr[i + range] = page + i;
			}
			return arr;
		}
	}
}
