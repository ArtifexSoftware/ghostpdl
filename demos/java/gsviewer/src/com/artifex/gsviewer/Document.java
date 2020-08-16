package com.artifex.gsviewer;

import static com.artifex.gsjava.GSAPI.*;

import java.awt.image.BufferedImage;
import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;
import java.util.Objects;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

import com.artifex.gsjava.GSAPI;
import com.artifex.gsjava.GSInstance;
import com.artifex.gsjava.callbacks.DisplayCallback;
import com.artifex.gsjava.util.BytePointer;
import com.artifex.gsjava.util.Reference;
import com.artifex.gsviewer.ImageUtil.ImageParams;

/**
 * <p>A Document stores an ordered list of Pages. This class implements
 * <code>java.util.List</code>, so it inherits all of a list's capabilities,
 * such as the ability to iterate over the document.</p>
 *
 * <p>The Document class also handles loading of pages through Ghostcript
 * and ensuring that two Ghostscript operations are not running at the
 * same time.</p>
 *
 * @author Ethan Vrhel
 *
 */
public class Document implements List<Page> {

	private static final DocumentLoader documentLoader = new DocumentLoader(); // The document loader
	private static final int format = GS_COLORS_RGB | GS_DISPLAY_DEPTH_8 | GS_DISPLAY_BIGENDIAN; // Format

	private static final ReentrantLock operationLock = new ReentrantLock(); // The operation lock
	private static final Condition operationCv = operationLock.newCondition(); // The operation condition variable
	private static volatile boolean operationInProgress = false; // Whether an operation is in progress

	/**
	 * Asynchronous execution dispatch returns.
	 */
	public static final int ASYNC_DISPATCH_OK = 0, ASYNC_DISPATCH_OPERATION_IN_PROGRESS = 1;

	/**
	 * Operation mode indicating the method should wait until a running operation has finished.
	 */
	public static final int OPERATION_WAIT = 0;

	/**
	 * Operation mode indicating the method should immediately return if an operation is already
	 * running.
	 */
	public static final int OPERATION_RETURN = 1;

	/**
	 * Operation mode indicating the method should throw an <code>OperationInProgressException</code>
	 * if an operation is already running.
	 */
	public static final int OPERATION_THROW = 2;

	/**
	 * Loads a document on a separate thread.
	 *
	 * @param file The file to load.
	 * @param cb The callback when loading the document has finished.
	 * @param dlb The callback signifying when progress has occurred while loading.
	 * @param operationMode How the loader should behave if an operation is already in
	 * progress. Can be <code>OPERATION_WAIT</code>, meaning the loader should
	 * wait until a running operation has finished, <code>OPERATION_RETURN</code> meaning
	 * the loader should immediately return <code>ASYNC_DISPATCH_OPERATION_IN_PROGRESS</code>, or
	 * <code>OPERATION_THROW</code> meaning the loader should throw an
	 * <code>OperationInProgressException</code>.
	 *
	 * @return The state of the thread dispatching. This is either <code>ASYNC_DISPATCH_OK</code>
	 * or <code>ASYNC_DISPATCH_OPERATION_IN_PROGRESS</code> which is also dependent on
	 * <code>operationMode</code>.
	 *
	 * @throws NullPointerException When <code>cb</code> is <code>null</code>.
	 * @throws RuntimeException When the current thread fails to wait until a thread has finished
	 * completing an operation.
	 * @throws IllegalArgumentException When <code>operationMode</code> is not
	 * <code>OPERTAION_WAIT</coded>, <code>OPERATION_RETURN</code>, or <code>OPERATION_THROW</code>.
	 * @throws OperationInProgressException When an operation is in progress and
	 * <code>operationMode</codeE> is <code>OPERATION_THROW</code>.
	 */
	public static int loadDocumentAsync(final File file, final DocAsyncLoadCallback cb,
			final DocLoadProgressCallback dlb, final int operationMode)
			throws NullPointerException, RuntimeException, IllegalArgumentException,
			OperationInProgressException {
		if (!handleOperationMode(operationMode))
			return ASYNC_DISPATCH_OPERATION_IN_PROGRESS;

		Objects.requireNonNull(cb, "DocAsyncLoadCallback");
		final Thread t = new Thread(() -> {
			Document doc = null;
			Exception exception = null;
			try {
				doc = new Document(file, dlb, operationMode);
			} catch (FileNotFoundException | IllegalStateException | NullPointerException e) {
				exception = e;
			}
			cb.onDocumentLoad(doc, exception);
		});
		t.setName("Document-Loader-Thread");
		t.setDaemon(true);
		t.start();
		return ASYNC_DISPATCH_OK;
	}

	/**
	 * Loads a document on a separate thread.
	 *
	 * @param file The file to load.
	 * @param cb The callback when loading the document has finished.
	 * @param operationMode How the loader should behave if an operation is already in
	 * progress. Can be <code>OPERATION_WAIT</code>, meaning the loader should
	 * wait until a running operation has finished, <code>OPERATION_RETURN</code> meaning
	 * the loader should immediately return <code>ASYNC_DISPATCH_OPERATION_IN_PROGRESS</code>, or
	 * <code>OPERATION_THROW</code> meaning the loader should throw an
	 * <code>OperationInProgressException</code>.
	 *
	 * @return The state of the thread dispatching. This is either <code>ASYNC_DISPATCH_OK</code>
	 * or <code>ASYNC_DISPATCH_OPERATION_IN_PROGRESS</code> which is also dependent on
	 * <code>operationMode</code>.
	 *
	 * @throws NullPointerException When <code>cb</code> is <code>null</code>.
	 * @throws RuntimeException When the current thread fails to wait until a thread has finished
	 * completing an operation.
	 * @throws IllegalArgumentException When <code>operationMode</code> is not
	 * <code>ASYNC_OPERTAION_WAIT</coded> or <code>ASYNC_OPERATION_RETURN</code>.
	 */
	public static int loadDocumentAsync(final File file, final DocAsyncLoadCallback cb,
			final int operationMode)
			throws NullPointerException, RuntimeException, IllegalArgumentException {
		return loadDocumentAsync(file, cb, null, operationMode);
	}

	/**
	 * Loads a document on a separate thread.
	 *
	 * @param filename The name of the file to load.
	 * @param cb The callback when loading the document has finished.
	 * @param dlb The callback signifying when progress has occurred while loading.
	 * @param operationMode How the loader should behave if an operation is already in
	 * progress. Can be <code>OPERATION_WAIT</code>, meaning the loader should
	 * wait until a running operation has finished, <code>OPERATION_RETURN</code> meaning
	 * the loader should immediately return <code>ASYNC_DISPATCH_OPERATION_IN_PROGRESS</code>, or
	 * <code>OPERATION_THROW</code> meaning the loader should throw an
	 * <code>OperationInProgressException</code>.
	 *
	 * @return The state of the thread dispatching. This is either <code>ASYNC_DISPATCH_OK</code>
	 * or <code>ASYNC_DISPATCH_OPERATION_IN_PROGRESS</code> which is also dependent on
	 * <code>operationMode</code>.
	 *
	 * @throws NullPointerException When <code>cb</code> is <code>null</code>.
	 * @throws RuntimeException When the current thread fails to wait until a thread has finished
	 * completing an operation.
	 * @throws IllegalArgumentException When <code>operationMode</code> is not
	 * <code>ASYNC_OPERTAION_WAIT</coded> or <code>ASYNC_OPERATION_RETURN</code>.
	 */
	public static int loadDocumentAsync(final String filename, final DocAsyncLoadCallback cb,
			final DocLoadProgressCallback dlb, final int operationMode)
			throws NullPointerException, RuntimeException, IllegalArgumentException {
		return loadDocumentAsync(new File(filename), cb, dlb, operationMode);
	}

	/**
	 * Loads a document on a separate thread.
	 *
	 * @param filename The name of the file to load.
	 * @param cb The callback when loading the document has finished.
	 * @param operationMode How the loader should behave if an operation is already in
	 * progress. Can be <code>OPERATION_WAIT</code>, meaning the loader should
	 * wait until a running operation has finished, <code>OPERATION_RETURN</code> meaning
	 * the loader should immediately return <code>ASYNC_DISPATCH_OPERATION_IN_PROGRESS</code>, or
	 * <code>OPERATION_THROW</code> meaning the loader should throw an
	 * <code>OperationInProgressException</code>.
	 *
	 * @return The state of the thread dispatching. This is either <code>ASYNC_DISPATCH_OK</code>
	 * or <code>ASYNC_DISPATCH_OPERATION_IN_PROGRESS</code> which is also dependent on
	 * <code>operationMode</code>.
	 *
	 * @throws NullPointerException When <code>cb</code> is <code>null</code>.
	 * @throws RuntimeException When the current thread fails to wait until a thread has finished
	 * completing an operation.
	 * @throws IllegalArgumentException When <code>operationMode</code> is not
	 * <code>ASYNC_OPERTAION_WAIT</coded> or <code>ASYNC_OPERATION_RETURN</code>.
	 */
	public static int loadDocumentAsync(final String filename, final DocAsyncLoadCallback cb,
			final int operationMode)
			throws NullPointerException, RuntimeException, IllegalArgumentException {
		return loadDocumentAsync(filename, cb, null, operationMode);
	}

	/**
	 * Call, internally, on the start of an operation.
	 */
	private static void startOperation() {
		operationInProgress = true;
	}

	/**
	 * Call, internally, on the start of an operation, before <code>startOperation()</code>.
	 *
	 * @param operationMode The operation mode.
	 * @return Whether the calling function should continue to its operation.
	 * @throws OperationInProgressException When an operation is in progress and <code>operationMode</code>
	 * is <code>OPERATION_THROW</code>. This should not be caught by the calling function, who should
	 * throw it instead.
	 * @throws IllegalArgumentException When <code>operationMode</code> is not a valid operation mode.
	 */
	private static boolean handleOperationMode(final int operationMode)
			throws OperationInProgressException, IllegalArgumentException {
		if (operationInProgress && operationMode == OPERATION_RETURN)
			return false;
		else if (operationInProgress && operationMode == OPERATION_THROW) {
			StackTraceElement[] elems = new Exception().getStackTrace();
			throw new OperationInProgressException(elems[1].getClassName() + "." + elems[1].getMethodName());
		} else if (operationInProgress && operationMode == OPERATION_WAIT)
			waitForOperation();
		else if (operationInProgress)
			throw new IllegalArgumentException("Unknown operation mode: " + operationMode);
		return true;
	}

	/**
	 * Called internally by <code>handleOperationMode()</code>. Waits for an operation
	 * to finish.
	 */
	private static void waitForOperation() {
		operationLock.lock();
		try {
			operationCv.await();
		} catch (InterruptedException e) {
			throw new RuntimeException("Thread interrupted", e);
		} finally {
			operationLock.unlock();
		}
	}

	/**
	 * Call, internally, when an operation is done.
	 */
	private static void operationDone() {
		operationLock.lock();
		operationInProgress = false;
		try {
			operationCv.signal();
		} finally {
			operationLock.unlock();
		}
	}

	@FunctionalInterface
	public static interface DocAsyncLoadCallback {

		/**
		 * Called when a document has finished loading on a separate
		 * thread.
		 *
		 * @param doc The document which was loaded. <code>null</code> if the document
		 * failed to load.
		 * @param exception An exception that ocurred while loading, <code>null</code>
		 * if no exception was thrown.
		 */
		public void onDocumentLoad(Document doc, Exception exception);
	}

	@FunctionalInterface
	public static interface DocLoadProgressCallback {

		/**
		 * Called when the document has made progress while loading.
		 *
		 * @param progress The amount of progress (from 0 to 100).
		 */
		public void onDocumentProgress(int progress);
	}

	/**
	 * Exception indicating an operation is already in progress.
	 *
	 * @author Ethan Vrhel
	 *
	 */
	public static final class OperationInProgressException extends RuntimeException {

		private static final long serialVersionUID = 1L;

		public OperationInProgressException(String message) {
			super(message);
		}
	}

	private static GSInstance initDocInstance() throws IllegalStateException {
		GSInstance instance = new GSInstance();

		int code;
		code = instance.set_arg_encoding(1);
		if (code != GS_ERROR_OK) {
			instance.delete_instance();
			throw new IllegalStateException("Failed to set arg encoding (code = " + code + ")");
		}

		code = instance.set_display_callback(documentLoader.reset());
		if (code != GS_ERROR_OK) {
			instance.delete_instance();
			throw new IllegalStateException("Failed to set display callback (code = " + code + ")");
		}

		String[] gsargs = new String[] {
			"gs",
			"-dNOPAUSE", "-dSAFER", "-I%rom%Resource%/Init/", "-dBATCH",
			"-sDEVICE=display",
			"-dFirstPage=1",
			"-dDisplayFormat=" + format
		};
		code = instance.init_with_args(gsargs);
		if (code != GS_ERROR_OK) {
			instance.delete_instance();
			throw new IllegalStateException("Failed to init with args (code = " + code + ")");
		}

		return instance;
	}

	/**
	 * Class which handles loading a Document.
	 *
	 * @author Ethan Vrhel
	 *
	 */
	private static class DocumentLoader extends DisplayCallback {
		private int pageWidth, pageHeight, pageRaster;
		private BytePointer pimage;
		private List<BufferedImage> images;
		private int progress;
		private DocLoadProgressCallback callback;

		private DocumentLoader() {
			reset();
		}

		private DocumentLoader reset() {
			this.pageWidth = 0;
			this.pageHeight = 0;
			this.pageRaster = 0;
			this.pimage = null;
			this.images = new LinkedList<>();
			this.progress = 0;
			this.callback = null;
			return this;
		}

		@Override
		public int onDisplayPresize(long handle, long device, int width, int height, int raster, int format) {
			System.out.println("Presize");
			return 0;
		}

		@Override
		public int onDisplaySize(long handle, long device, int width, int height, int raster, int format,
				BytePointer pimage) {
			System.out.println("Size");
			this.pageWidth = width;
			this.pageHeight = height;
			this.pageRaster = raster;
			this.pimage = pimage;
			return 0;
		}

		@Override
		public int onDisplayPage(long handle, long device, int copies, boolean flush) {
			byte[] data = (byte[]) pimage.toArrayNoConvert();
			images.add(ImageUtil.createImage(data, new ImageParams(pageWidth, pageHeight, pageRaster, BufferedImage.TYPE_3BYTE_BGR)));

			progress += (100 - progress) / 2;
			if (callback != null)
				callback.onDocumentProgress(progress);

			return 0;
		}

		@Override
		public void finalize() {
			System.err.println("WARNING: The document loader has been freed by the Java VM");
		}
	}

	private File file;
	private List<Page> pages;
	private GSInstance gsInstance;

	/**
	 * Creates and loads a new document.
	 *
	 * @param file The file to load.
	 * @param loadCallback The callback to indicate when progress has occurred while loading.
	 * @param operationMode How the loader should behave if an operation is already in
	 * progress. Can be <code>OPERATION_WAIT</code>, meaning the loader should
	 * wait until a running operation has finished, <code>OPERATION_RETURN</code> meaning
	 * the loader should immediately return, or <code>OPERATION_THROW</code> meaning the loader
	 * should throw an <code>OperationInProgressException</code>.
	 *
	 * @throws FileNotFoundException When <code>file</code> does not exist.
	 * @throws IllegalStateException When Ghostscript fails to initialize or load the document.
	 * @throws NullPointerException When <code>file</code> is <code>null</code>.
	 * @throws OperationInProgressException When an operation is already in progress
	 * and <code>operationMode</code> is <code>OPERATION_THROW</code>.
	 * @throws IllegalArgumentException When <code>operationMode</code> is not
	 * <code>OPERATION_WAIT</coded>, <code>OPERATION_RETURN</code>, or <code>OPERATION_THROW</code>.
	 */
	public Document(final File file, final DocLoadProgressCallback loadCallback, int operationMode)
			throws FileNotFoundException, IllegalStateException, NullPointerException,
			OperationInProgressException, IllegalArgumentException {
		this.file = Objects.requireNonNull(file, "file");
		if (!file.exists())
			throw new FileNotFoundException(file.getAbsolutePath());

		final String[] gargs = { "gs", "-dNOPAUSE", "-dSAFER", "-I%rom%Resource%/Init/", "-dBATCH", "-r" + Page.PAGE_LOW_DPI,
				"-sDEVICE=display", "-dDisplayFormat=" + format, "-f", file.getAbsolutePath() };

		if (!handleOperationMode(operationMode))
			return;

		startOperation();

		try {
			gsInstance = initDocInstance();
		} catch (IllegalStateException e) {
			operationDone();
		}

		if (gsInstance == null) {
			operationDone();
			throw new IllegalStateException("Failed to initialize Ghostscript");
		}

		documentLoader.callback = loadCallback;

		int code = gsInstance.set_param("HWResolution", Page.PAGE_LOW_DPI_STR, GS_SPT_PARSED);
		if (code != GS_ERROR_OK) {
			operationDone();
			throw new IllegalStateException("Failed to set HWResolution (code = " + code + ")");
		}

		Reference<Integer> exitCode = new Reference<>();
		code = gsInstance.run_file(file.getAbsolutePath(), 0, exitCode);

		if (code != GS_ERROR_OK) {
			operationDone();
			throw new IllegalStateException("Failed to run file (code = " + code + ")");
		}

		gsInstance.set_param("TextAlphaBits", 4, GS_SPT_INT);
		gsInstance.set_param("GraphicsAlphaBits", 4, GS_SPT_INT);

		//gsInstance.exit();
		//gsInstance.delete_instance();

		/*int code = instance.init_with_args(gargs);
		instance.exit();
		instance.delete_instance();
		if (code != GS_ERROR_OK) {
			operationDone();
			throw new IllegalStateException("Failed to gsapi_init_with_args code=" + code);
		}*/

		this.pages = new ArrayList<>(documentLoader.images.size());
		for (BufferedImage img : documentLoader.images) {
			pages.add(new Page(img));
		}

		if (documentLoader.callback != null)
			documentLoader.callback.onDocumentProgress(100);

		operationDone();
	}

	/**
	 * Creates and loads a document from a filename.
	 *
	 * @param file The file to load.
	 * @param operationMode How the loader should behave if an operation is already in
	 * progress. Can be <code>OPERATION_WAIT</code>, meaning the loader should
	 * wait until a running operation has finished, <code>OPERATION_RETURN</code> meaning
	 * the loader should immediately return, or <code>OPERATION_THROW</code> meaning the loader
	 * should throw an <code>OperationInProgressException</code>.
	 *
	 * @throws FileNotFoundException When <code>file</code> does not exist.
	 * @throws IllegalStateException When Ghostscript fails to initialize or load the document.
	 * @throws NullPointerException When <code>file</code> is <code>null</code>.
	 * @throws OperationInProgressException When an operation is already in progress
	 * and <code>operationMode</code> is <code>OPERATION_THROW</code>.
	 * @throws IllegalArgumentException When <code>operationMode</code> is not
	 * <code>OPERATION_WAIT</coded>, <code>OPERATION_RETURN</code>, or <code>OPERATION_THROW</code>.
	 */
	public Document(final File file, final int operationMode)
			throws FileNotFoundException, IllegalStateException, NullPointerException,
			OperationInProgressException {
		this(file, null, operationMode);
	}

	/**
	 * Creates and loads a document from a filename.
	 *
	 * @param filename The name of the file to load.
	 * @param loadCallback The callback to indicate when progress has occurred while loading.
	 * @param operationMode How the loader should behave if an operation is already in
	 * progress. Can be <code>OPERATION_WAIT</code>, meaning the loader should
	 * wait until a running operation has finished, <code>OPERATION_RETURN</code> meaning
	 * the loader should immediately return, or <code>OPERATION_THROW</code> meaning the loader
	 * should throw an <code>OperationInProgressException</code>.
	 *
	 * @throws FileNotFoundException When <code>file</code> does not exist.
	 * @throws IllegalStateException When Ghostscript fails to initialize or load the document.
	 * @throws NullPointerException When <code>file</code> is <code>null</code>.
	 * @throws OperationInProgressException When an operation is already in progress
	 * and <code>operationMode</code> is <code>OPERATION_THROW</code>.
	 * @throws IllegalArgumentException When <code>operationMode</code> is not
	 * <code>OPERATION_WAIT</coded>, <code>OPERATION_RETURN</code>, or <code>OPERATION_THROW</code>.
	 */
	public Document(final String filename, final DocLoadProgressCallback loadCallback,
			final int operationMode)
			throws FileNotFoundException, IllegalStateException, NullPointerException,
			OperationInProgressException {
		this(new File(Objects.requireNonNull(filename, "filename")), loadCallback, operationMode);
	}

	/**
	 * Creates and loads a document from a filename.
	 *
	 * @param filename The name of the file to load.
	 * @param operationMode How the loader should behave if an operation is already in
	 * progress. Can be <code>OPERATION_WAIT</code>, meaning the loader should
	 * wait until a running operation has finished, <code>OPERATION_RETURN</code> meaning
	 * the loader should immediately return, or <code>OPERATION_THROW</code> meaning the loader
	 * should throw an <code>OperationInProgressException</code>.
	 *
	 * @throws FileNotFoundException When <code>filename</code> does not exist.
	 * @throws IllegalStateException When Ghostscript fails to initialize or load the document.
	 * @throws NullPointerException When <code>filename</code> is <code>null</code>.
	 * @throws OperationInProgressException When an operation is already in progress
	 * and <code>operationMode</code> is <code>OPERATION_THROW</code>.
	 * @throws IllegalArgumentException When <code>operationMode</code> is not
	 * <code>OPERATION_WAIT</coded>, <code>OPERATION_RETURN</code>, or <code>OPERATION_THROW</code>.
	 */
	public Document(final String filename, final int operationMode)
			throws FileNotFoundException, IllegalStateException, NullPointerException,
			OperationInProgressException {
		this(filename, null, operationMode);
	}

	/**
	 * Loads the high resolution images in a range of images.
	 *
	 * @param operationMode How the loader should behave if an operation is already in
	 * progress. Can be <code>OPERATION_WAIT</code>, meaning the loader should
	 * wait until a running operation has finished, <code>OPERATION_RETURN</code> meaning
	 * the loader should immediately return, or <code>OPERATION_THROW</code> meaning the loader
	 * should throw an <code>OperationInProgressException</code>.
	 * @param startPage The first page to load.
	 * @param endPage The end page to load.
	 *
	 * @throws IndexOutOfBoundsException When <code>firstPage</code> or <code>endPage</code>
	 * are not in the document or <code>endPage</code> is less than <code>firstPage</code>.
	 * @throws IllegalStateException When Ghostscript fails to initialize or load the document.
	 */
	public void loadHighRes(int operationMode, int startPage, int endPage)
			throws IndexOutOfBoundsException, IllegalStateException {
		checkBounds(startPage, endPage);

		if (!handleOperationMode(operationMode))
			return;

		startOperation();

		final String[] gargs = { "gs", "-dNOPAUSE", "-dSAFER", "-I%rom%Resource%/Init/",
				"-dBATCH", "-r" + Page.PAGE_HIGH_DPI, "-sDEVICE=display",
				"-dFirstPage=" + startPage, "-dLastPage=" + endPage, "-dDisplayFormat=" + format,
				"-dTextAlphaBits=4", "-dGraphicsAlphaBits=4",
				"-f", file.getAbsolutePath() };

		/*GSInstance instance = null;
		try {
			instance = initDocInstance();
		} catch (IllegalStateException e) {
			operationDone();
		}

		if (instance == null)
			throw new IllegalStateException("Failed to initialize Ghoscript");

		int code = instance.init_with_args(gargs);
		instance.exit();
		instance.delete_instance();
		if (code != GS_ERROR_OK) {
			operationDone();
			throw new IllegalStateException("Failed to gsapi_init_with_args code=" + code);
		}*/
		//gsInstance.set_display_callback(documentLoader.reset());
		//documentLoader.reset();
		documentLoader.images.clear();

		this.setParams(Page.PAGE_HIGH_DPI, startPage, endPage);

		for (GSInstance.GSParam<?> param : gsInstance) {
			System.out.println(param);
		}

		Reference<Integer> ref = new Reference<>();
		gsInstance.get_param_once("FirstPage", ref, GS_SPT_INT);
		System.out.println(ref);
		gsInstance.get_param_once("LastPage", ref, GS_SPT_INT);
		System.out.println(ref);

		Reference<Integer> exitCode = new Reference<>();
		//gsInstance.set_display_callback(documentLoader.reset());
		int code = gsInstance.run_file(file.getAbsolutePath(), 0, exitCode);

		if (code != GS_ERROR_OK) {
			operationDone();
			throw new IllegalStateException("Failed to run file (code = " + code + ")");
		}

		if (documentLoader.images.size() != endPage - startPage + 1) {
			operationDone();
			throw new IllegalStateException("Page range mismatch (expected " +
					(endPage - startPage) + ", got " + documentLoader.images.size() + ")");
		}

		int ind = startPage - 1;
		for (final BufferedImage img : documentLoader.images) {
			this.pages.get(ind++).setHighRes(img);
		}

		operationDone();
	}

	/**
	 * Loads the high resolution image of a singular page.
	 *
	 * @param operationMode How the loader should behave if an operation is already in
	 * progress. Can be <code>OPERATION_WAIT</code>, meaning the loader should
	 * wait until a running operation has finished, <code>OPERATION_RETURN</code> meaning
	 * the loader should immediately return, or <code>OPERATION_THROW</code> meaning the loader
	 * should throw an <code>OperationInProgressException</code>.
	 * @param page The page to load.
	 *
	 * @throws IndexOutOfBoundsException When <code>page</code> is not in the document.
	 * @throws IllegalStateException When Ghostscript fails to initialize or load the document.
	 * @throws OperationInProgressException When an operation is already in progress
	 * and <code>operationMode</code> is <code>OPERATION_THROW</code>.
	 * @throws IllegalArgumentException When <code>operationMode</code> is not
	 * <code>OPERATION_WAIT</coded>, <code>OPERATION_RETURN</code>, or <code>OPERATION_THROW</code>.
	 */
	public void loadHighRes(int operationMode, int page)
			throws IndexOutOfBoundsException, IllegalStateException, OperationInProgressException {
		loadHighRes(operationMode, page, page);
	}

	/**
	 * Loads the high resolution images of a list of pages.
	 *
	 * @param operationMode How the loader should behave if an operation is already in
	 * progress. Can be <code>OPERATION_WAIT</code>, meaning the loader should
	 * wait until a running operation has finished, <code>OPERATION_RETURN</code> meaning
	 * the loader should immediately return, or <code>OPERATION_THROW</code> meaning the loader
	 * should throw an <code>OperationInProgressException</code>.
	 * @param pages The pages to load.
	 *
	 * @throws IndexOutOfBoundsException When any page is not a page in the document.
	 * @throws IllegalStateException When Ghostscript fails to initialize or load the document.
	 * @throws OperationInProgressException When an operation is already in progress
	 * and <code>operationMode</code> is <code>OPERATION_THROW</code>.
	 * @throws IllegalArgumentException When <code>operationMode</code> is not
	 * <code>OPERATION_WAIT</coded>, <code>OPERATION_RETURN</code>, or <code>OPERATION_THROW</code>.
	 */
	public void loadHighResList(int operationMode, final int[] pages)
			throws IndexOutOfBoundsException, IllegalStateException, OperationInProgressException {
		if (pages.length > 0) {
			if (!handleOperationMode(operationMode))
				return;

			try {
				startOperation();

				final StringBuilder builder = new StringBuilder();
				if (pages[0] < 1 || pages[0] > this.pages.size())
					throw new IndexOutOfBoundsException("page=" + pages[0]);
				builder.append(pages[0]);

				for (int i = 1; i < pages.length; i++) {
					if (pages[i] < 1 || pages[i] > this.pages.size())
						throw new IndexOutOfBoundsException("page=" + pages[i]);
					builder.append(',').append(pages[i]);
				}


				final String[] gargs = { "gs", "-dNOPAUSE", "-dSAFER", "-I%rom%Resource%/Init/", "-dBATCH", "-r" + Page.PAGE_HIGH_DPI,
						"-sDEVICE=display", "-sPageList=" + builder.toString(), "-dDisplayFormat=" + format,
						"-dTextAlphaBits=4", "-dGraphicsAlphaBits=4",
						"-f", file.getAbsolutePath() };

				GSInstance instance = null;
				try {
					instance = initDocInstance();
				} catch (IllegalStateException e) {
					operationDone();
				}

				if (instance == null)
					throw new IllegalStateException("Failed to initialize Ghoscript");

				int code = instance.init_with_args(gargs);
				instance.exit();
				instance.delete_instance();
				if (code != GS_ERROR_OK) {
					throw new IllegalStateException("Failed to gsapi_init_with_args code=" + code);
				}

				int ind = 0;
				for (final BufferedImage img : documentLoader.images) {
					this.pages.get(pages[ind] - 1).setHighRes(img);
				}
			} finally {
				operationDone();
			}
		}
	}

	/**
	 * Unloads the high resolution images in a range of pages.
	 *
	 * @param startPage The start page to unload the high resolution image from.
	 * @param endPage The end page to unload the high resolution image from.
	 *
	 * @throws IndexOutOfBoundsException When <code>startPage</code> and <code>endPage</code>
	 * are outside document or <code>endPage</code> is less than <code>startPage</code>.
	 */
	public void unloadHighRes(int startPage, int endPage) throws IndexOutOfBoundsException {
		checkBounds(startPage, endPage);
		for (int i = startPage - 1; i < endPage; i++) {
			pages.get(i).unloadHighRes();
		}
	}

	/**
	 * Unloads a high resolution image at the given page.
	 *
	 * @param page The page to unload the high resolution image.
	 *
	 * @throws IndexOutOfBoundsException When page is not a page in the document.
	 */
	public void unloadHighRes(int page) throws IndexOutOfBoundsException {
		unloadHighRes(page, page);
	}

	/**
	 * Unloads the zoomed images for a range of pages.
	 *
	 * @param startPage The start page to unload.
	 * @param endPage The end page to unload.
	 *
	 * @throws IndexOutOfBoundsException When <code>startPage</code> and <code>endPage</code>
	 * are outside document or <code>endPage</code> is less than <code>startPage</code>.
	 */
	public void unloadZoomed(int startPage, int endPage) throws IndexOutOfBoundsException {
		checkBounds(startPage, endPage);
		for (int i = startPage; i <= endPage; i++) {
			pages.get(i).unloadZoomed();
		}
	}

	/**
	 * Unloads a zoomed image for a page.
	 *
	 * @param page The page to unload.
	 *
	 * @throws IndexOutOfBoundsException When <code>page</code> is not in the document.
	 */
	public void unloadZoomed(int page) throws IndexOutOfBoundsException {
		unloadZoomed(page, page);
	}

	/**
	 * Unloads the document's resources. The document will be unusable after this
	 * call.
	 */
	public void unload() {
		for (Page p : pages) {
			p.unloadAll();
		}
		pages.clear();
	}

	/**
	 * Returns the name of the document loaded.
	 *
	 * @return The name.
	 */
	public String getName() {
		return file.getName();
	}

	/**
	 * Zooms the area around page <code>page</code>. The area around <code>page</code>
	 * is <code>page - 1</code> to <code>page + 1</code>. The method will automatically
	 * handle if <code>page - 1</code> or <code>page + 1</code> are out of range. However,
	 * it does not handle if <code>page</code> is not in the document.
	 *
	 * @param operationMode How the loader should behave if an operation is already in
	 * progress. Can be <code>OPERATION_WAIT</code>, meaning the loader should
	 * wait until a running operation has finished, <code>OPERATION_RETURN</code> meaning
	 * the loader should immediately return, or <code>OPERATION_THROW</code> meaning the loader
	 * should throw an <code>OperationInProgressException</code>.
	 * @param page The page to load around.
	 * @param zoom The zoom of the pages.
	 *
	 * @throws IndexOutOfBoundsException When <code>page</code> is not in the document.
	 * @throws IllegalStateException When Ghostscript fails to initialize or load the document.
	 * @throws OperationInProgressException When an operation is already in progress
	 * and <code>operationMode</code> is <code>OPERATION_THROW</code>.
	 * @throws IllegalArgumentException When <code>operationMode</code> is not
	 * <code>OPERATION_WAIT</coded>, <code>OPERATION_RETURN</code>, or <code>OPERATION_THROW</code>.
	 */
	public void zoomArea(final int operationMode, final int page, final double zoom)
		throws IndexOutOfBoundsException {
		checkBounds(page, page);

		if (!handleOperationMode(operationMode))
			return;

		try {
			startOperation();

			int start = page - 1;
			start = start < 1 ? page : start;
			int end = page + 1;
			end = end > pages.size() ? page : end;

			final String[] gargs = {
					"gs", "-dNOPAUSE", "-dSAFER", "-I%rom%Resource%/Init/",
					"-dBATCH", "-r" + (int)(Page.PAGE_HIGH_DPI * zoom),
					"-sDEVICE=display", "-dFirstPage=" + start, "-dLastPage=" + end,
					"-dDisplayFormat=" + format,
					"-dTextAlphaBits=4", "-dGraphicsAlphaBits=4",
					"-f", file.getAbsolutePath() };

			GSInstance instance = null;
			try {
				instance = initDocInstance();
			} catch (IllegalStateException e) {
				operationDone();
			}

			if (instance == null)
				throw new IllegalStateException("Failed to initialize Ghoscript");

			int code = instance.init_with_args(gargs);
			instance.exit();
			instance.delete_instance();
			if (code != GS_ERROR_OK) {
				operationDone();
				throw new IllegalStateException("Failed to gsapi_init_with_args code=" + code);
			}

			int ind = start - 1;
			for (final BufferedImage img : documentLoader.images) {
				this.pages.get(ind++).setZoomed(img);
			}
		} finally {
			operationDone();
		}
	}

	/**
	 * Checks to make sure the pages <code>start</code> through
	 * <code>end</code> are in the document, and throws an
	 * <code>IndexOutOfBoundsException</code> if they are not.
	 *
	 * @param start The start page.
	 * @param end The end page.
	 * @throws IndexOutOfBoundsException When <code>startPage</code> and <code>endPage</code>
	 * are outside document or <code>endPage</code> is less than <code>startPage</code>.
	 */
	private void checkBounds(int start, int end) throws IndexOutOfBoundsException {
		if (start < 1 || start > pages.size())
			throw new IndexOutOfBoundsException("start=" + start);
		if (end < 1 || end > pages.size())
			throw new IndexOutOfBoundsException("end=" + end);
		if (end < start)
			throw new IndexOutOfBoundsException("end < start");
	}

	private void setParams(int dpi, int startPage, int lastPage) {
		int code = gsInstance.set_param("HWResolution", Page.toDPIString(dpi), GS_SPT_PARSED);
		if (code != GS_ERROR_OK) {
			operationDone();
			throw new IllegalStateException("Failed to set HWResolution (code = " + code + ")");
		}

		code = gsInstance.set_param("FirstPage", startPage, GS_SPT_INT);
		if (code != GS_ERROR_OK) {
			operationDone();
			throw new IllegalStateException("Failed to set dFirstPage (code = " + code + ")");
		}

		code = gsInstance.set_param("LastPage", lastPage, GS_SPT_INT);
		if (code != GS_ERROR_OK) {
			operationDone();
			throw new IllegalStateException("Failed to set dEndPage (code = " + code + ")");
		}
	}

	// Implementations of inherited methods from java.util.List.

	@Override
	public int size() {
		return pages.size();
	}

	@Override
	public boolean isEmpty() {
		return pages.isEmpty();
	}

	@Override
	public boolean contains(Object o) {
		return pages.contains(o);
	}

	@Override
	public Iterator<Page> iterator() {
		return pages.iterator();
	}

	@Override
	public Object[] toArray() {
		return pages.toArray();
	}

	@Override
	public <T> T[] toArray(T[] a) {
		return pages.toArray(a);
	}

	@Override
	public boolean add(Page e) {
		return pages.add(e);
	}

	@Override
	public boolean remove(Object o) {
		return pages.remove(o);
	}

	@Override
	public boolean containsAll(Collection<?> c) {
		return pages.containsAll(c);
	}

	@Override
	public boolean addAll(Collection<? extends Page> c) {
		return pages.addAll(c);
	}

	@Override
	public boolean addAll(int index, Collection<? extends Page> c) {
		return pages.addAll(index, c);
	}

	@Override
	public boolean removeAll(Collection<?> c) {
		return pages.removeAll(c);
	}

	@Override
	public boolean retainAll(Collection<?> c) {
		return pages.retainAll(c);
	}

	@Override
	public void clear() {
		pages.clear();
	}

	@Override
	public Page get(int index) {
		return pages.get(index);
	}

	@Override
	public Page set(int index, Page element) {
		return pages.set(index, element);
	}

	@Override
	public void add(int index, Page element) {
		pages.add(index, element);
	}

	@Override
	public Page remove(int index) {
		return pages.remove(index);
	}

	@Override
	public int indexOf(Object o) {
		return pages.indexOf(o);
	}

	@Override
	public int lastIndexOf(Object o) {
		return pages.lastIndexOf(o);
	}

	@Override
	public ListIterator<Page> listIterator() {
		return pages.listIterator();
	}

	@Override
	public ListIterator<Page> listIterator(int index) {
		return pages.listIterator(index);
	}

	@Override
	public List<Page> subList(int fromIndex, int toIndex) {
		return pages.subList(fromIndex, toIndex);
	}

	@Override
	public String toString() {
		return "Document[numPages=" + size() + "]";
	}

	@Override
	public boolean equals(Object o) {
		if (o == this)
			return true;
		if (o instanceof Document) {
			return pages.equals(((Document)o).pages);
		}
		return false;
	}

	@Override
	public int hashCode() {
		return pages.hashCode();
	}

	@Override
	public void finalize() {
		unload();
	}
}
