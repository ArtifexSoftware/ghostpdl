import java.io.File;

import com.artifex.gsjava.GSInstance;
import com.artifex.gsjava.callbacks.DisplayCallback;
import com.artifex.gsjava.util.*;
import com.artifex.gsjava.callbacks.*;

import java.io.IOException;

import static com.artifex.gsjava.GSAPI.*;

public class Worker implements Runnable {

	public static final boolean USE_STDOUT = false;

	private static int ID = 0;

	private final File inPS, outPDF;
	private final Thread thread;

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

			StdIO io = new StdIO();
			gsInstance.set_stdio(io, null, null);

			if (!outPDF.exists())
				outPDF.createNewFile();

			final int FORMAT = GS_COLORS_RGB | GS_DISPLAY_DEPTH_8 | GS_DISPLAY_BIGENDIAN;
			String[] args = {
				"gs",
				"-sDEVICE=pdfwrite",
				//"dFirstPage=1",
				//"dDisplayFormat=" + FORMAT,
				"-o", outPDF.getAbsolutePath(),
				"-f", inPS.getAbsolutePath()
			};

			int code = gsInstance.init_with_args(args);
			if (code != GS_ERROR_OK) {
				gsInstance.delete_instance();
				throw new IllegalStateException("Failed to init with args (code = " + code + ")");
			}

			System.out.println("Worker " + thread.getName() + " completed.");
		} catch (Exception e) {
			System.err.println("Worker " + thread.getName() + " threw exception: " + e);
		} finally {
			if (gsInstance != null)
				gsInstance.delete_instance();
		}
	}

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

	private static class StdIO implements IStdOutFunction, IStdErrFunction, IStdInFunction {

		public int onStdOut(long callerHandle, byte[] str, int len) {
			//if (USE_STDOUT)
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