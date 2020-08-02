package com.artifex.gsviewer;

import java.io.ByteArrayOutputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.PrintStream;

public class DefaultUnhandledExceptionHandler implements Thread.UncaughtExceptionHandler {

	public static final DefaultUnhandledExceptionHandler INSTANCE;

	static {
		INSTANCE = new DefaultUnhandledExceptionHandler();
		Thread.setDefaultUncaughtExceptionHandler(INSTANCE);
	}

	private DefaultUnhandledExceptionHandler() {

	}

	@Override
	public void uncaughtException(Thread t, Throwable e) {
		StringBuilder builder = new StringBuilder();

		builder.append("Unhandled exception!\n");
		builder.append("Exception: " + e.getClass().getName() + "\n");
		builder.append("Message: " + e.getMessage() + "\n");
		builder.append("Cause: " + (e.getCause() == null ? "[Unknown]" :
			e.getCause().getClass().getName()) + "\n");
		builder.append("Thread: " + t.getName() + " (id=" + t.getId() + ")\n");
		builder.append("Stack trace:" + "\n");
		builder.append(stackTraceToString(e));
		String fullMessage = builder.toString();
		System.err.println(fullMessage);
		try {
			PrintStream out = new PrintStream(new FileOutputStream("error.log"));
			out.println(fullMessage);
			out.close();
		} catch (FileNotFoundException e1) {
			System.err.println("Failed to write to log file");
		}
		System.exit(1);
	}

	private String stackTraceToString(Throwable e) {
		ByteArrayOutputStream os = new ByteArrayOutputStream();
		PrintStream out = new PrintStream(os);
		e.printStackTrace(out);
		return new String(os.toByteArray());
	}
}
