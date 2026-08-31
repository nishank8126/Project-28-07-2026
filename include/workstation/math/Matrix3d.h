// RE_PENDING (Piece 4A cleanup). Matrix3d is NOT part of the verified M1-M6
// slice. 3x3 determinant / inverse / rotation factories are unauthorized
// textbook linear algebra and have been removed. They may only be added once a
// specific workstation routine (e.g. a recovered 3x3 inverse) is reversed.
// Evidence: see docs/reverse_engineering/EvidenceLedger.md (E30 / RE_PENDING).
