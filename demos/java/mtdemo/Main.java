import com.artifex.gsjava.GSInstance;

import java.io.File;

import java.util.Scanner;

public class Main {

	// The file we will read from
	public static final String INFILE = "../../../examples/tiger.eps";

	// The output directory
	public static final String OUTDIR = "pdfout";

	public static void main(String[] args) {
		// For multithreading, call this before any GSInstance objects
		// are created.
		GSInstance.setAllowMultithreading(true);

		// Parse first command line argument as thread count
		int workerCount = 10;
		if (args.length > 0) {
			try {
				workerCount = Integer.parseInt(args[0]);
			} catch (NumberFormatException e) { }
		}

		// Create output directory if it doesn't exist
		File outdirFile = new File(OUTDIR);
		if (outdirFile.exists()) {
			if (outdirFile.isFile())
				System.err.println("Output directory exists as a file!");
		} else {
			outdirFile.mkdirs();
		}


		Worker[] workers = new Worker[workerCount];

		// Create each worker
		for (int i = 0; i < workers.length; i++) {
			workers[i] = new Worker(INFILE, OUTDIR + "/out" + i + ".pdf");
		}

		// Dispatch each worker
		System.out.println("Starting workers...");
		for (int i = 0; i < workers.length; i++) {
			workers[i].start();
		}
	}
}
