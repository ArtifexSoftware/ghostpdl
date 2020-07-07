package com.artifex.gsjava.callbacks;

@FunctionalInterface
public interface ICalloutFunction {

	public int onCallout(long instance, long calloutHandle, byte[] deviceName, int id, int size, long data);
}
