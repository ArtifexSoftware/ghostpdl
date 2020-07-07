package com.artifex.gsjava.callbacks;

@FunctionalInterface
public interface IStdInFunction {

	public int onStdIn(long callerHandle, byte[] buf, int len);
}
