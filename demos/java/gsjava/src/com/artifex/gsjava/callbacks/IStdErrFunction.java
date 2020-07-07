package com.artifex.gsjava.callbacks;

@FunctionalInterface
public interface IStdErrFunction {

	public int onStdErr(long callerHandle, byte[] str, int len);
}
