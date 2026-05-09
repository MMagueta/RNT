#pragma once

enum OBJECT_TYPE {
    MULTIGROUP,
    RELATION,
    TUPLE,
    ATTRIBUTE,
    TRANSACTION
};

enum METHOD {
    OPEN,
    CLOSE,
    PARSE, // To plug a secondary namespace
    SECURITY
};

enum AUTH_CLAIM {
    READ,
    WRITE
};

enum AUTH_METHOD {
    CERTIFICATE,
    PLAIN_TEXT
};
