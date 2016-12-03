--TEST--
Check for protocol buffers sfixed64 implementations
--SKIPIF--
<?php if (version_compare(PHP_VERSION, "5.3.0") < 0) { die('skip for 5.2'); } ?>
--FILE--
<?php

$target  = new ProtocolBuffers\FieldDescriptor();
if ($target  instanceof ProtocolBuffersFieldDescriptor) {
	echo "OK" . PHP_EOL;
} else {
	echo get_class($target) . PHP_EOL;
}


--EXPECT--
OK
