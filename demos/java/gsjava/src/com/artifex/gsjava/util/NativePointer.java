package com.artifex.gsjava.util;

import java.io.File;

/**
 * <p>The NativePointer class allows for direct non-managed memory
 * allocations. The purpose of this class is to provide direct access
 * to memory blocks.</p>
 *
 * <p>An instance of the class is safe in the sense that
 * no memory can be accessed that is filled with junk which has been
 * freed, and a check is performed so that in the worst case scenario,
 * only a Java exception will be thrown. However, an instance of the
 * class can cause memory leaks if not used correctly as garbage collecting
 * the object will not free the memory natively, so it will need to be
 * maintained.</p>
 *
 * <p>This class also contains several native methods which can be used
 * for slightly faster native access to unmanaged memory. These methods
 * are considered unsafe, and for read/write access to the memory,
 * an implementation <code>NativeArray</code> should be used.</code>.
 *
 * @author Ethan Vrhel
 *
 */
public class NativePointer {

	static {
		registerLibraries();
	}

	/**
	 * Registers the needed native libraries.
	 */
	private static void registerLibraries() {
		try {
			// Try loading normally
			System.loadLibrary("gs_jni");
		} catch (UnsatisfiedLinkError e) {
			// Load using absolute paths
			if (System.getProperty("os.name").equalsIgnoreCase("Linux")) {
				// Load on Linux
				File libgpdl = new File("libgpdl.so");
				System.load(libgpdl.getAbsolutePath());
				File gsjni = new File("gs_jni.so");
				System.load(gsjni.getAbsolutePath());
			} else if (System.getProperty("os.name").equalsIgnoreCase("Mac OS X")) {
				// Load on Mac
				File libgpdl = new File("libgpdl.dylib");
				System.load(libgpdl.getAbsolutePath());
				File gsjni = new File("gs_jni.dylib");
				System.load(gsjni.getAbsolutePath());
			} else {
				throw e;
			}
		}
	}

	public static final long	NULL = 0x0L;

	public static final long	BYTE_SIZE = 1L,
								CHAR_SIZE = 2L,
								SHORT_SIZE = 2L,
								INT_SIZE = 4L,
								LONG_SIZE = 8L;

	private long address;

	/**
	 * Constructs a null native pointer.
	 */
	public NativePointer() {
		address = NULL;
	}

	/**
	 * Allocates a memory block of size <code>size</code>.
	 *
	 * @param size The size of the block, in bytes.
	 * @throws IllegalStateException When this pointer already points to something.
	 * @throws AllocationError When the native allocation fails.
	 */
	public final void malloc(long size) throws IllegalStateException, AllocationError {
		if (address != NULL) {
			throw new IllegalStateException("Memory already allocated.");
		}
		this.address = mallocNative(size);
	}

	/**
	 * Allocates an block of size <code>count</code> * <code>size</code> with each
	 * byte initialized to 0.
	 *
	 * @param count The number of elements to allocated.
	 * @param size The size of each element, in bytes.
	 * @throws IllegalStateException When this pointer already points to something.
	 * @throws AllocationError When the native allocation fails.
	 */
	public final void calloc(long count, long size) throws IllegalStateException, AllocationError {
		if (address != NULL) {
			throw new IllegalStateException("Memory already allocated.");
		}
		this.address = callocNative(count, size);
	}

	/**
	 * Frees the allocated block, if any has been allocated.
	 */
	public final void free() {
		freeNative(address);
		address = NULL;
	}

	/**
	 * Returns the native address of this pointer.
	 *
	 * @return The native address.
	 */
	public final long getAddress() {
		return address;
	}

	@Override
	public boolean equals(Object o) {
		if (o == this)
			return true;
		if (o instanceof NativePointer) {
			return address == ((NativePointer)o).address;
		}
		return false;
	}

	@Override
	public String toString() {
		return new StringBuilder().append("0x").append(Long.toHexString(address)).toString();
	}

	@Override
	public int hashCode() {
		return (int)address;
	}

	private static native long mallocNative(long size) throws AllocationError;

	private static native long callocNative(long count, long size) throws AllocationError;

	private static native void freeNative(long block);


	public static native byte[] byteArrayNative(long address, long len);

	public static native byte byteAtNative(long address, long index);

	public static native void setByteNative(long address, long index, byte value);

	public static native char[] charArrayNative(long address, long len);

	public static native char charAtNative(long address, long index);

	public static native void setCharNative(long address, long index, char value);

	public static native short[] shortArrayNative(long address, long len);

	public static native short shortAtNative(long address, long index);

	public static native void setShortNative(long address, long index, short value);

	public static native int[] intArrayNative(long address, long len);

	public static native int intAtNative(long address, long index);

	public static native void setIntNative(long address, long index, int value);

	public static native long[] longArrayNative(long address, long len);

	public static native long longAtNative(long address, long index);

	public static native void setLongNative(long address, long index, long value);
}
