import java.io.File;

import com.artifex.gsjava.GSInstance;
import com.artifex.gsjava.callbacks.DisplayCallback;
import com.artifex.gsjava.util.*;
import com.artifex.gsjava.callbacks.*;

import java.io.IOException;

import java.util.Arrays;

import static com.artifex.gsjava.GSAPI.*;

/**
 * Handles running a separate instance of Ghostscript.
 */
public class Worker implements Runnable {

	// Whether to use Java-implemented standard input/output
	public static final boolean USE_CUSTOM_STDIO = true;

	// Whether stdout is enabled (effective only if USE_CUSTOM_STDIO = true)
	public static final boolean USE_STDOUT = false;

	private static int ID = 0; // Next thread ID

	private final File inPS, outPDF;	// Input and output files
	private final Thread thread;		// The thread running this worker

	/**
	 * Create a worker thread to convert a postscript file
	 * to a pdf file.
	 *
	 * @param inPS The postscript file.
	 * @param outPDF The output PDF file.
	 */
	public Worker(String inPS, String outPDF) {
		this.inPS = new File(inPS);
		this.outPDF = new File(outPDF);
		this.thread = new Thread(this);
		this.thread.setName("Worker-" + ID++);
	}

	/**
	 * Starts this worker
	 */
	public void start() {
		System.out.println("Start: " + thread.getName());
		thread.start();
	}

	@Override
	public void run() {
		System.out.println("Started worker: " + thread.getName());

		GSInstance gsInstance = null;
		try {
			gsInstance = createGSInstance();

			// If we want to use the IO functions StdIO, set them
			if (USE_CUSTOM_STDIO) {
				StdIO io = new StdIO();
				gsInstance.set_stdio(io, USE_STDOUT ? io : null, io);
			}

			// Create the output file if it doesn't exist
			if (!outPDF.exists())
				outPDF.createNewFile();

			// Ghostscript arguments to use with init_with_args
			String[] args = {
				"gs",
				"-sDEVICE=pdfwrite",
				"-o", outPDF.getPath(),
				inPS.getPath()
			};

			// init_with_args will perform all the conversions
			int code = gsInstance.init_with_args(args);
			if (code != GS_ERROR_OK)
				throw new IllegalStateException("Failed to init with args (code = " + code + ")");

			System.out.println("Worker " + thread.getName() + " completed.");
		} catch (Exception e) {
			System.err.println("Worker " + thread.getName() + " threw exception: " + e);
		} finally {
			if (gsInstance != null)
				gsInstance.delete_instance();
		}
	}

	// Creates a new Ghostscript inistance
	private GSInstance createGSInstance() {
		GSInstance gsInstance = new GSInstance();

		int code;
		code = gsInstance.set_arg_encoding(1);
		if (code != GS_ERROR_OK) {
			gsInstance.delete_instance();
			throw new IllegalStateException("Failed to set arg encoding (code = " + code + ")");
		}

		return gsInstance;
	}

	// Handles standard input/output
	private static class StdIO implements IStdOutFunction, IStdErrFunction, IStdInFunction {

		public int onStdOut(long callerHandle, byte[] str, int len) {
			System.out.println(new String(str));
			return len;
		}

		public int onStdErr(long callerHandle, byte[] str, int len) {
			System.err.println(new String(str));
			return len;
		}

		public int onStdIn(long callerHandle, byte[] buf, int len) {
			try {
				return System.in.read(buf, 0, len);
			} catch (IOException e) {
				return 0;
			}
		}
	}
}
