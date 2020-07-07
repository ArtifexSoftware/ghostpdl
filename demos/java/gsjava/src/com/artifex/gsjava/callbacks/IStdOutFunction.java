package com.artifex.gsjava.callbacks;

@FunctionalInterface
public interface IStdOutFunction {

	public int onStdOut(long callerHandle, byte[] str, int len);
}
