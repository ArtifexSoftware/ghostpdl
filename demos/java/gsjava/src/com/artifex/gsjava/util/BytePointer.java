package com.artifex.gsjava.util;

/**
 * A byte pointer stores a pointer to a memory location containing
 * bytes.
 *
 * @author Ethan Vrhel
 *
 */
public class BytePointer extends NativePointer implements NativeArray<Byte> {

	private long length;

	public BytePointer() {
		super();
		this.length = 0;
	}

	@Override
	public void allocate(long length) throws IllegalStateException, AllocationError {
		calloc(length, BYTE_SIZE);
		this.length = length;
	}

	@Override
	public void set(long index, Byte value) throws IndexOutOfBoundsException, NullPointerException {
		final long address = getAddress();
		if (address == NULL)
			throw new NullPointerException("address is null");
		if (index < 0 || index >= length)
			throw new IndexOutOfBoundsException(new StringBuilder().append("index: ").append(index).toString());
		setByteNative(address, index, value);
	}

	@Override
	public Byte at(long index) throws IndexOutOfBoundsException, NullPointerException {
		final long address = getAddress();
		if (address == NULL)
			throw new NullPointerException("address is null");
		if (index < 0 || index >= length)
			throw new IndexOutOfBoundsException(new StringBuilder().append("index: ").append(index).toString());
		return byteAtNative(address, index);
	}

	@Override
	public Byte[] toArray() throws NullPointerException {
		byte[] byteArray = (byte[])toArrayNoConvert();
		Byte[] bArray = new Byte[byteArray.length];
		int i = 0;
		for (byte b : byteArray)
			bArray[i++] = b;
		return bArray;
	}

	@Override
	public Object toArrayNoConvert() throws NullPointerException {
		final long address = getAddress();
		if (address == NULL)
			throw new NullPointerException("address is null");
		return byteArrayNative(address, length);
	}

	@Override
	public long length() {
		return length;
	}

}
