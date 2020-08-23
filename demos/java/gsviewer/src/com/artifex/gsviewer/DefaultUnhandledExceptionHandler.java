package com.artifex.gsviewer;

import java.io.ByteArrayOutputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.Enumeration;
import java.util.Properties;

/**
 * The default unhandled exception handler. This will handle exceptions which
 * are not caught in a try/catch block and print an error message to
 * <code>System.err</code> and a log file.
 *
 * @author Ethan Vrhel
 *
 */
public class DefaultUnhandledExceptionHandler implements Thread.UncaughtExceptionHandler {

	/**
	 * An instance of the exception handler.
	 */
	public static final DefaultUnhandledExceptionHandler INSTANCE;

	static {
		INSTANCE = new DefaultUnhandledExceptionHandler();
		Thread.setDefaultUncaughtExceptionHandler(INSTANCE);
	}

	private DefaultUnhandledExceptionHandler() { }

	@Override
	public void uncaughtException(Thread t, Throwable e) {
		StringBuilder builder = new StringBuilder();

		DateTimeFormatter dtf = DateTimeFormatter.ofPattern("yyyy/MM/dd HH:mm:ss");
		builder.append("Unhandled exception!\n");

		// Print the time of the error
		builder.append("Time: " + dtf.format(LocalDateTime.now()) + "\n\n");

		// Print all system properties
		builder.append("PROPERTIES\n");
		Properties props = System.getProperties();
		for (Enumeration<Object> keys = props.keys(); keys.hasMoreElements();) {
			String key = keys.nextElement().toString();
			String prop = props.getProperty(key);
			String friendly = "\"";
			for (int i = 0; i < prop.length(); i++) {
				char c = prop.charAt(i);
				switch (c) {
				case '\n':
					friendly += "\\n";
					break;
				case '\r':
					friendly += "\\r";
					break;
				default:
					friendly += c;
					break;
				}
			}
			friendly += "\"";
			builder.append(key + " = " + friendly + "\n");
		}
		builder.append('\n');

		// Print the stack trace of the error
		builder.append("EXCEPTION\n");
		builder.append("Exception: " + e.getClass().getName() + "\n");
		builder.append("Message: " + e.getMessage() + "\n");
		builder.append("Cause: " + (e.getCause() == null ? "[Unknown]" :
			e.getCause().getClass().getName()) + "\n");
		builder.append("Thread: " + t.getName() + " (id=" + t.getId() + ")\n");
		builder.append("Stack trace:" + "\n");
		builder.append(stackTraceToString(e));

		// Write to System.err and a log file
		String fullMessage = builder.toString();
		System.err.println(fullMessage);
		try {
			PrintStream out = new PrintStream(new FileOutputStream("error.log"));
			out.println(fullMessage);
			out.close();
		} catch (FileNotFoundException e1) {
			System.err.println("Failed to write to log file");
		}

		// Close the program
		System.exit(1);
	}

	/**
	 * Converts a stack trace to a string.
	 *
	 * @param e The <code>Throwable</code> whose stack trace should be converted.
	 * @return The stack trace as a string.
	 */
	private String stackTraceToString(Throwable e) {
		ByteArrayOutputStream os = new ByteArrayOutputStream();
		PrintStream out = new PrintStream(os);
		e.printStackTrace(out);
		return new String(os.toByteArray());
	}
}
