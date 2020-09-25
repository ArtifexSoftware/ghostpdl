package com.artifex.gsjava.callbacks;

@FunctionalInterface
public interface IPollFunction {

	public int onPoll(long callerHandle);
}
