package gstest;

import static com.artifex.gsjava.GSAPI.*;

import java.io.File;

import com.artifex.gsjava.GSAPI;
import com.artifex.gsjava.GSParam;
import com.artifex.gsjava.GSParams;
import com.artifex.gsjava.util.*;

public class Main {

	public static void main(String[] args) {
		LongReference iref = new LongReference();
		int code;
		if ((code = gsapi_new_instance(iref, GS_NULL)) != GS_ERROR_OK)
			throw new IllegalStateException("new instance returned " + code);

		long instance = iref.value;

		gsapi_set_arg_encoding(instance, 1);

		String[] gsargs = new String[] {
			"gs",
			"-sDEVICE=pngalpha",
			"-r100",
		};

		gsapi_init_with_args(instance, gsargs);

		Reference<?> aaRef = new Reference<>();
		if ((code = gsapi_get_param_once(instance, "TextAlphaBits", aaRef, GS_SPT_INT)) < 0) {
			throw new IllegalStateException("gsapi_get_param_once returned " + code);
		}
		System.out.println("TextAlpaBits=" + aaRef.getValue());

		if ((code = gsapi_set_param(instance, "TextAlphaBits", 1, GS_SPT_INT)) < 0) {
			throw new IllegalStateException("gsapi_set_param returned " + code);
		}

		/*if ((code = gsapi_get_param_once(instance, "TextAlphaBits", aaRef, GS_SPT_INT)) < 0) {
			throw new IllegalStateException("gsapi_get_param_once returned " + code);
		}
		System.out.println("TextAlpaBits=" + aaRef.getValue());*/

		NativePointer value = new NativePointer();
		int bytes = gsapi_get_param(instance, "TextAlphaBits", NativePointer.NULL, GS_SPT_INT);
		value.malloc(bytes);
		code = gsapi_get_param(instance, "TextAlphaBits", value.getAddress(), GS_SPT_INT);
		int val = NativePointer.intAtNative(value.getAddress(), 0);
		System.out.println(val);
		value.free();

		gsapi_delete_instance(instance);
	}
}
