package com.artifex.gsjava.callbacks;

@FunctionalInterface
public interface IStdOutFunction {

	/**
	 * Called when something should be written to the standard
	 * output stream.
	 *
	 * @param callerHandle The caller handle.
	 * @param str The string to write.
	 * @param len The number of bytes to write.
	 * @return The number of bytes written, must be <code>len</code>.
	 */
	public int onStdOut(long callerHandle, byte[] str, int len);
}
