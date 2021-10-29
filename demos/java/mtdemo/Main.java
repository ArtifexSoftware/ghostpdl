import com.artifex.gsjava.GSInstance;

public class Main {

	public static void main(String[] args) {
		GSInstance.setAllowMultithreading(true);

		int workerCount = 10;
		if (args.length > 0) {
			try {
				workerCount = Integer.parseInt(args[0]);
			} catch (NumberFormatException e) { }
		}

		Worker[] workers = new Worker[workerCount];

		for (int i = 0; i < workers.length; i++) {
			workers[i] = new Worker("../../../examples/tiger.eps", "pdfout/out" + i + ".pdf");
		}

		System.out.println("Starting workers...");
		for (int i = 0; i < workers.length; i++) {
			workers[i].start();
		}
		System.out.println("Workers dispatched");
	}
}