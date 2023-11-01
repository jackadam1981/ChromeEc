use super::{flash::StatusRegister, gscvd};
use ap_ro_verification_config::WriteProtectDescriptor;
use ti50_macros::{cfg_host_attr, enum_as};
use ti50_syscalls::sys_mgr::ApRoVerificationTpmvStatus;

/// The result type used throughout AP RO verification.
pub type Result<T> = core::result::Result<T, VerifyError>;

/// Represents success or failure of AP RO verification
#[derive(Copy, Clone)]
pub struct ApRoVerificationResult(Result<()>);

impl ApRoVerificationResult {
    /// Value that represents success value both in long life scratch and detailed UMA
    /// value. We do not want to use `0`, `!0`, or any value that aliases to a detailed
    /// error, which is verified by unit test.
    const SERIALIZED_SUCCESS: u32 = 0xFFFF_F000;

    /// Returns true if the AP RO verification successfully passed
    pub fn is_success(self) -> bool {
        self.0.is_ok()
    }
}

impl From<ApRoVerificationResult> for ApRoVerificationTpmvStatus {
    fn from(value: ApRoVerificationResult) -> Self {
        value
            .0
            .map_or_else(|e| e.into(), |()| ApRoVerificationTpmvStatus::Success)
    }
}

impl From<u32> for ApRoVerificationResult {
    fn from(value: u32) -> Self {
        Self(if value == Self::SERIALIZED_SUCCESS {
            Ok(())
        } else {
            Err(
                VerifyError::try_from(value).unwrap_or(VerifyError::Internal(
                    InternalErrorSource::CouldNotDeserialize,
                    0,
                )),
            )
        })
    }
}

impl From<Result<()>> for ApRoVerificationResult {
    fn from(value: Result<()>) -> Self {
        Self(value)
    }
}

impl From<ApRoVerificationResult> for Result<()> {
    fn from(value: ApRoVerificationResult) -> Self {
        value.0
    }
}

impl From<ApRoVerificationResult> for u32 {
    fn from(value: ApRoVerificationResult) -> Self {
        value
            .0
            .map_or_else(u32::from, |()| ApRoVerificationResult::SERIALIZED_SUCCESS)
    }
}

/// An error that occured verifying the AP RO contents.
///
/// One byte contains a top-level error code of [`VerifyErrorCode`].
/// Three bytes contain a detailed status.
///
/// This value has a niche at 0, so `Result<(), VerifyError>`
/// can represent `Ok` as a 0, saving space.
#[cfg_host_attr(derive(Debug, Eq, PartialEq))]
#[repr(C, align(4))]
#[derive(Clone, Copy)]
pub struct VerifyError {
    code: VerifyErrorCode,
    /// Details data that is stored big endian byte order, where the most significant
    /// value should be first in the array.
    detail: [u8; 3],
}

impl VerifyError {
    const fn new(code: VerifyErrorCode) -> Self {
        Self {
            code,
            detail: [0; 3],
        }
    }

    #[inline(always)]
    pub const fn with_detail(mut self, detail: [u8; 3]) -> Self {
        self.detail = detail;
        self
    }

    #[inline(always)]
    pub fn code(self) -> VerifyErrorCode {
        self.code
    }

    pub fn detail(self) -> [u8; 3] {
        self.detail
    }

    /// Failures in validating the write protect status registers.
    #[inline(always)]
    #[allow(non_snake_case)]
    pub const fn FailedStatusRegister(
        register: StatusRegister,
        got: u8,
        expected: WriteProtectDescriptor,
    ) -> Self {
        let code = match register {
            StatusRegister::Register1 => VerifyErrorCode::FailedStatusRegister1,
            StatusRegister::Register2 => VerifyErrorCode::FailedStatusRegister2,
            StatusRegister::Register3 => VerifyErrorCode::FailedStatusRegister3,
        };
        // Invalid descriptors are represented with masks of 0, as this would normally pass.
        let (value, mask) = match expected.get() {
            Ok((value, mask)) => (value, mask),
            Err(e) => (e as u8, 0),
        };
        Self::new(code).with_detail([got, value, mask])
    }

    pub fn with_failed_signature_location(mut self, location: SignatureLocation) -> Self {
        self.detail[1] = location as u8;
        self
    }
}

impl From<VerifyError> for u32 {
    fn from(value: VerifyError) -> Self {
        // The error code should be the most significant byte, so the detailed code
        // can be bound a range of values that belong to a top-level VerifyErrorCode.
        // Details are stored as BE bytes.
        u32::from_be_bytes([
            value.code as u8,
            value.detail[0],
            value.detail[1],
            value.detail[2],
        ])
    }
}

/// Indicates that input data could be not converted to a valid [`VerifyError`].
pub struct InvalidVerifyError;

impl TryFrom<u32> for VerifyError {
    type Error = InvalidVerifyError;

    fn try_from(value: u32) -> core::result::Result<Self, Self::Error> {
        let [code, d0, d1, d2] = value.to_be_bytes();
        VerifyErrorCode::from_u8(code)
            .map(|e| VerifyError::new(e).with_detail([d0, d1, d2]))
            .ok_or(InvalidVerifyError)
    }
}

/// Generates [`VerifyError`] constants that use a given [`VerifyErrorCode`] name
/// and contain no details.
///
/// The capitalization is intended to mirror enum variants with no value.
macro_rules! no_detail_codes {
    ($($code:ident),* $(,)?) => {
        #[allow(non_upper_case_globals)]
        impl VerifyError {
            $(
                #[doc=concat!(
                    "Error with the [`VerifyErrorCode::", stringify!($code),
                    "`] code and empty details")
                ]
                pub const $code: Self = Self::new(VerifyErrorCode:: $code);
            )*
        }
    }
}

/// Generates [`VerifyError`] functions that use a given [`VerifyErrorCode`] name
/// and describe the arguments and transformation to a `[u8; 3]` of details.
///
/// The capitalization is intended to mirror enum tuple variants.
macro_rules! detailed_codes {
    ($($code:ident( $($arg:ident: $t:ty),* $(,)?) => $e:expr),* $(,)?) => {
        #[allow(non_snake_case)]
        impl VerifyError {
            $(
                #[doc=concat!(
                    "Error with the [`VerifyErrorCode::", stringify!($code),
                    "`] code and specified details")
                ]
                #[inline(always)]
                pub const fn $code ( $($arg: $t),* ) -> Self {
                    Self::new(VerifyErrorCode:: $code).with_detail($e)
                }
            )*
        }
    };
}

no_detail_codes!(
    // TODO(b/263298180): detail: lots of choices, what was wrong?
    InconsistentGscvd,
    // This is currently only used for if the keyblock is too big.
    InconsistentKeyblock,
    // TODO(b/263298180): detail: which key was bad, platform/root
    InconsistentKey,
    // TODO(b/263298180): detail: Command that failed? offset/len?
    SpiRead,
    // Possible detail: `SyncCell` static that increments for every alloc.
    OutOfMemory,
    // TODO(b/263298180): detail: How too big was it, and what was too big?
    // Also, consider subsuming `InconsistentKeyblock`
    TooBig,
    // TODO(b/263298180): detail: where did we look?
    MissingGscvd,
    // This will only trigger if the either none or just SPI settings are not provisioned.
    SettingNotProvisioned,
    // TODO(b/263298180): detail: contents of GBB flags from `GbbHeader`
    NonZeroGbbFlags,
    WrongRootKey,
);

detailed_codes!(
    FailedVerification(details: FailedVerificationDetail) => details.to_details(),
    VersionMismatch(source: VersionMismatchSource) => [source as u8, 0, 0],
    UnsupportedCryptoAlgorithm(source: CryptoAlgorithmSource, alg: u16) => {
        let [hi, lo] = alg.to_be_bytes();
        [source as u8, hi, lo]
    },
    Internal(source: InternalErrorSource, code: u16) => {
        let [hi, lo] = code.to_be_bytes();
        [source as u8, hi, lo]
    },
    // `got` and `expected` both get 3 nibbles of data
    BoardIdMismatch(got: u32, expected: u32) => {
        let [_, a, b, c] = (got >> 20 << 12 | expected >> 20).to_be_bytes();
        [a, b, c]
    },
);

/// Details for the [`VerifyErrorCode::Internal`] error.
///
/// Values cannot change, they can only be appended.
#[enum_as(u8)]
pub enum InternalErrorSource {
    /// The lower bytes are a [`kernel::ErrorCode`].
    Kernel = 1,

    /// The lower bytes are a [`hil::crypto::CryptoError`].
    /// Note the values are all negative.hi, lo
    Crypto = 2,

    /// Should never happen.
    VerifyFlashRanges = 3,

    /// The cached AP RO verification result value was not parsable into a valid result again
    CouldNotDeserialize = 4,
}

/// Details for the [`VerifyErrorCode::VersionMismatch`] error. Values cannot change, they can only
/// be appended.
#[cfg_host_attr(derive(Debug))]
#[enum_as(u8)]
pub enum VersionMismatchSource {
    Gscvd = 1,
    Keyblock = 2,
}

/// Details for the [`VerifyErrorCode::UnsupportedCryptoAlgorithm`] error.
///
/// Values cannot change, they can only be appended.
#[cfg_host_attr(derive(Debug))]
#[enum_as(u8)]
pub enum CryptoAlgorithmSource {
    Gscvd = 1,

    /// A `Vb2PackedKey`, either the root key or platform key
    ///
    /// TODO(b/263298180): disambiguate; requires setting after-the-fact
    Vb2PackedKey = 4,
}

/// Details for the [`VerifyErrorCode::FailedVerification`] error.
///
/// Values cannot change, they can only be appended.
#[cfg_host_attr(derive(Debug))]
pub enum FailedVerificationDetail {
    DigestMismatch {
        location: DigestLocation,
        got: u8,
        expected: u8,
    },
    SignatureVerifyFail,
}

/// Part of the top byte of the detail for [`VerifyErrorCode::FailedVerification`].
///
/// Values cannot change, they can only be appended.
#[cfg_host_attr(derive(Debug))]
#[enum_as(u8)]
pub enum SignatureLocation {
    Gscvd = 1,
    Keyblock = 2,
}

/// Part of the top byte of the detail for [`VerifyErrorCode::FailedVerification`].
///
/// Max value is 15.
/// Values cannot change, they can only be appended.
#[cfg_host_attr(derive(Debug))]
#[enum_as(u8)]
pub enum DigestLocation {
    /// The protected regions of AP flash covered by the GSCVD.
    ProtectedRegions = 1,

    /// The cache for the GSCVD header.
    GvdCache = 2,

    /// The root key
    RootKey = 3,
}

impl FailedVerificationDetail {
    const DIGEST_MISMATCH: u8 = 0x10;
    const SIGNATURE_VERIFY: u8 = 0;

    #[inline(always)]
    const fn to_details(self) -> [u8; 3] {
        match self {
            FailedVerificationDetail::DigestMismatch {
                location,
                got,
                expected,
            } => {
                const _CHECK_MAX_DIGEST_LOCATION: () = assert!(DigestLocation::END <= 0x0f);
                const _CHECK_NUM_ROOT_KEY_HASHES: () = assert!(
                    gscvd::NUM_VALID_ROOT_KEY_HASHES < 14,
                    "Too many root key hashes to fill in detail unambiguously"
                );

                // Also record the (hardcoded) number of valid root key hashes.
                // The top byte carries three pieces of info:
                // - The top nibble being non-zero indicates this is a `DIGEST_MISMATCH`.
                // - The top nibble is 1 + NUM_VALID_ROOT_KEY_HASHES.
                // - The bottom nibble is the `DigestLocation`.
                let top_byte = FailedVerificationDetail::DIGEST_MISMATCH
                    + ((gscvd::NUM_VALID_ROOT_KEY_HASHES as u8) << 4)
                    + location as u8;
                [top_byte, got, expected]
            }
            FailedVerificationDetail::SignatureVerifyFail => {
                // Code structure requires the location be set after-the-fact.
                // See [`VerifyError::with_failed_signature_location`].
                [FailedVerificationDetail::SIGNATURE_VERIFY, 0, 0]
            }
        }
    }
}

/// All of the verification errors that can occur while validating AP RO. Values cannot change,
/// they can only be appended.
#[enum_as(u8)]
#[cfg_host_attr(derive(Debug))]
pub enum VerifyErrorCode {
    /// Consistent verification data was found, but it failed cryptographic verification.
    FailedVerification = 1,

    /// Failed WP status register verification on [`StatusRegister::Register1`].
    FailedStatusRegister1 = 2,

    /// Failed WP status register verification on [`StatusRegister::Register2`].
    FailedStatusRegister2 = 3,

    /// Failed WP status register verification on [`StatusRegister::Register3`].
    FailedStatusRegister3 = 4,

    /// The [`gscvd::GscVerificationData`] wasn't correctly laid out.
    InconsistentGscvd = 5,

    /// The [`keyblock::Vb2Keyblock`] wasn't correctly laid out.
    InconsistentKeyblock = 6,

    /// The key stored wasn't in a valid format.
    InconsistentKey = 7,

    /// A SPI read operation failed while communicating with AP flash.
    SpiRead = 8,

    /// The data uses a crypto algorithm that is unsupported.
    UnsupportedCryptoAlgorithm = 9,

    /// A structure version is unsupported.
    VersionMismatch = 10,

    /// There was not enough reserved memory to perform the operation as requested.
    OutOfMemory = 11,

    /// A miscellaneous internal error occurred.
    Internal = 12,

    /// A data structure was too large.
    TooBig = 13,

    /// There was no GSCVD present in flash.
    MissingGscvd = 14,

    /// The Board ID for this GSCVD is not correct for this board.
    BoardIdMismatch = 15,

    /// A necessary setting was not provisioned or was invalid.
    SettingNotProvisioned = 16,

    /// Verification failed solely because the GBB flags are non-zero.
    NonZeroGbbFlags = 17,

    /// The AP RO verification root key hash matched, but the key use is not
    /// authorized any more because only MP prod key is allowed at this point.
    WrongRootKey = 18,
}

// Ensure that all `VerifyErrorCode` values fit within 7 bits to ensure that the top bit
// is reserved for specifying if an error was seen after the success latch has been
// flipped. See `mix_latch_status_with_apro_status` for more details.
static_assertions::const_assert!(VerifyErrorCode::END < 0x80);

impl From<kernel::ErrorCode> for VerifyError {
    fn from(e: kernel::ErrorCode) -> Self {
        VerifyError::Internal(InternalErrorSource::Kernel, e as u16)
    }
}

impl From<hil::crypto::CryptoError> for VerifyError {
    fn from(e: hil::crypto::CryptoError) -> Self {
        VerifyError::Internal(InternalErrorSource::Crypto, e as u16)
    }
}

impl From<buf_arena::OutOfMemory> for VerifyError {
    fn from(_: buf_arena::OutOfMemory) -> Self {
        VerifyError::OutOfMemory
    }
}

impl From<VerifyError> for ApRoVerificationTpmvStatus {
    fn from(value: VerifyError) -> Self {
        value.code().into()
    }
}

impl From<VerifyErrorCode> for ApRoVerificationTpmvStatus {
    fn from(result: VerifyErrorCode) -> Self {
        use VerifyErrorCode::*;
        match result {
            FailedVerification
            | FailedStatusRegister1
            | FailedStatusRegister2
            | FailedStatusRegister3 => ApRoVerificationTpmvStatus::FailedVerification,
            InconsistentGscvd => ApRoVerificationTpmvStatus::InconsistentGscvd,
            InconsistentKeyblock => ApRoVerificationTpmvStatus::InconsistentKeyblock,
            InconsistentKey => ApRoVerificationTpmvStatus::InconsistentKey,
            SpiRead => ApRoVerificationTpmvStatus::SpiRead,
            UnsupportedCryptoAlgorithm => ApRoVerificationTpmvStatus::UnsupportedCryptoAlgorithm,
            VersionMismatch => ApRoVerificationTpmvStatus::VersionMismatch,
            OutOfMemory => ApRoVerificationTpmvStatus::OutOfMemory,
            Internal => ApRoVerificationTpmvStatus::Internal,
            TooBig => ApRoVerificationTpmvStatus::TooBig,
            MissingGscvd => ApRoVerificationTpmvStatus::MissingGscvd,
            BoardIdMismatch => ApRoVerificationTpmvStatus::BoardIdMismatch,
            SettingNotProvisioned => ApRoVerificationTpmvStatus::SettingNotProvisioned,
            // TODO(vbendeb): add handling of the below to gsctool.
            NonZeroGbbFlags => ApRoVerificationTpmvStatus::NonZeroGbbFlags,
            WrongRootKey => ApRoVerificationTpmvStatus::WrongRootkey,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_serialized_success_is_invalid_verification_failure() {
        assert!(
            VerifyError::try_from(ApRoVerificationResult::SERIALIZED_SUCCESS).is_err(),
            "Serialized success value cannot overlap with any valid verify error"
        );
    }

    #[test]
    fn test_zero_is_invalid_verification_failure() {
        // Block 0 from being the successful value since we serialize this to long life scratch, and
        // the power on reset value for it is 0
        assert!(
            !ApRoVerificationResult::from(0).is_success(),
            "0 must be a failure since 0 is PoR default for LLV scratch"
        );
    }

    #[test]
    fn test_setting_not_provisioned() {
        let error = VerifyError::SettingNotProvisioned;
        let error = u32::from(error);
        // 0x10 is the SettingNotProvisioned code,
        // 0x000000 is unused
        assert_eq!(error, 0x10_000000);
    }

    #[test]
    fn test_failed_verification() {
        let error = VerifyError::FailedVerification(FailedVerificationDetail::SignatureVerifyFail);
        let error = u32::from(error);
        // 0x01 is the FailedVerification code,
        // 0x00 is SignatureVerifyFail,
        // 0x0000 is unused
        assert_eq!(error, 0x01_00_0000);

        let error = VerifyError::FailedVerification(FailedVerificationDetail::DigestMismatch {
            location: DigestLocation::GvdCache,
            got: 0x11,
            expected: 0x22,
        });
        let error = u32::from(error);
        // 0x01 is the FailedVerification code,
        // 0x10 DigestMismatch + 0x20 is for num valid root keys + 0x02 for GvdCache
        // 0x11 is for got
        // 0x22 is for expected
        assert_eq!(error, 0x01_32_11_22);

        let error = VerifyError::FailedVerification(FailedVerificationDetail::DigestMismatch {
            location: DigestLocation::ProtectedRegions,
            got: 0x09,
            expected: 0x80,
        });
        let error = u32::from(error);
        // 0x01 is the FailedVerification code,
        // 0x10 DigestMismatch + 0x20 is for num valid root keys + 0x01 for ProtectedRegions
        // 0x09 is for got
        // 0x80 is for expected
        assert_eq!(error, 0x01_31_09_80);

        let error = VerifyError::FailedVerification(FailedVerificationDetail::DigestMismatch {
            location: DigestLocation::RootKey,
            got: 0x10,
            expected: 0x02,
        });
        let error = u32::from(error);
        // 0x01 is the FailedVerification code,
        // 0x10 DigestMismatch + 0x20 is for num valid root keys + 0x03 for Rootkey
        // 0x10 is for got
        // 0x02 is for expected
        assert_eq!(error, 0x01_33_10_02);

        let error = VerifyError::FailedVerification(FailedVerificationDetail::SignatureVerifyFail)
            .with_failed_signature_location(SignatureLocation::Gscvd);
        let error = u32::from(error);
        // 0x01 is the FailedVerification code,
        // 0x00 is SignatureVerifyFail,
        // 0x01 is Gscvd signature location
        // 0x00 is unused
        assert_eq!(error, 0x01_00_01_00);

        let error = VerifyError::FailedVerification(FailedVerificationDetail::SignatureVerifyFail)
            .with_failed_signature_location(SignatureLocation::Keyblock);
        let error = u32::from(error);
        // 0x01 is the FailedVerification code,
        // 0x00 is SignatureVerifyFail,
        // 0x02 is Keyblock signature location
        // 0x00 is unused
        assert_eq!(error, 0x01_00_02_00);
    }

    #[test]
    fn test_version_mismatch() {
        let error = VerifyError::VersionMismatch(VersionMismatchSource::Gscvd);
        let error = u32::from(error);
        // 0x0A is the VersionMismatch code,
        // 0x01 is VersionMismatchSource::Gscvd,
        // 0x0000 is unused
        assert_eq!(error, 0x0A_01_0000);

        let error = VerifyError::VersionMismatch(VersionMismatchSource::Keyblock);
        let error = u32::from(error);
        // 0x0A is the VersionMismatch code,
        // 0x02 is VersionMismatchSource::Keyblock,
        // 0x0000 is unused
        assert_eq!(error, 0x0A_02_0000);
    }

    #[test]
    fn test_unsupported_crypto_algorithm() {
        let error = VerifyError::UnsupportedCryptoAlgorithm(CryptoAlgorithmSource::Gscvd, 0x123);
        let error = u32::from(error);
        // 0x09 is the UnsupportedCryptoAlgorithm code,
        // 0x01 is CryptoAlgorithmSource::Gscvd,
        // 0x0123 is detailed code
        assert_eq!(error, 0x09_01_0123);

        let error =
            VerifyError::UnsupportedCryptoAlgorithm(CryptoAlgorithmSource::Vb2PackedKey, 0x3);
        let error = u32::from(error);
        // 0x09 is the UnsupportedCryptoAlgorithm code,
        // 0x04 is CryptoAlgorithmSource::Vb2PackedKey,
        // 0x003 is detailed code
        assert_eq!(error, 0x09_04_0003);
    }

    #[test]
    fn test_internal() {
        let error = VerifyError::Internal(InternalErrorSource::Crypto, 0x9876);
        let error = u32::from(error);
        // 0x0C is the Internal code,
        // 0x02 is InternalErrorSource::Crypto,
        // 0x9876 is detailed code
        assert_eq!(error, 0x0C_02_9876);

        let error = VerifyError::Internal(InternalErrorSource::Kernel, 0x0011);
        let error = u32::from(error);
        // 0x0C is the Internal code,
        // 0x01 is InternalErrorSource::Kernel,
        // 0x0011 is detailed code
        assert_eq!(error, 0x0C_01_0011);

        let error = VerifyError::Internal(InternalErrorSource::VerifyFlashRanges, 0x9900);
        let error = u32::from(error);
        // 0x0C is the Internal code,
        // 0x03 is InternalErrorSource::VerifyFlashRanges,
        // 0x9900 is detailed code
        assert_eq!(error, 0x0C_03_9900);
    }

    #[test]
    fn test_board_id_mismatch() {
        let error = VerifyError::BoardIdMismatch(0x01234567, 0x89abcdef);
        let error = u32::from(error);
        // 0x0F is the BoardIdMismatch code,
        // 0x012 is most significant 3 nibbles of got,
        // 0x89a is most significant 3 nibbles of expected
        assert_eq!(error, 0x0F_012_89a);
    }
}
