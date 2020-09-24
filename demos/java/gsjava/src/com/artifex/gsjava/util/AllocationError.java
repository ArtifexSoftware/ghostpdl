package com.artifex.gsjava.util;

/**
 * An error thrown when native memory allocation fails.
 *
 * @author Ethan Vrhel
 *
 */
public class AllocationError extends Error {

	private static final long serialVersionUID = 1L;

	public AllocationError() {
		super();
	}

	public AllocationError(String message) {
		super(message);
	}

	public AllocationError(String message, Throwable cause) {
		super(message, cause);
	}

	public AllocationError(Throwable cause) {
		super(cause);
	}

	protected AllocationError(String message, Throwable cause,
			boolean enableSuppression, boolean writableStackTrace) {
		super(message, cause, enableSuppression, writableStackTrace);
	}
}
