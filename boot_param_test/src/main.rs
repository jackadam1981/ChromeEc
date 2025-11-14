// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// boot_param_test: utility for testing BootParam produced by GSC
use anyhow::{anyhow, bail, Error, Result};

use ciborium::Value;
use coset::{iana, Algorithm, CborSerializable, CoseError, CoseKey, Label};

use diced_open_dice::{
    bcc_handover_main_flow, bcc_handover_parse, derive_cdi_private_key_seed, hash,
    keypair_from_seed, Config, DiceArtifacts, DiceContext, DiceError, DiceMode, Hash,
    Hidden, InputValues, KeyAlgorithm, HASH_SIZE, HIDDEN_SIZE, VM_KEY_ALGORITHM,
};

use hwtrust::session::{Options, RkpInstance, Session};
use hwtrust::{dice, dice::ChainForm, rkp};

const MAX_NEXT_HANDOVER_SIZE: usize = 4096;

fn err_str<T>(str: &'static str) -> Result<T> {
    Err(anyhow!(str))
}

fn print_help() -> Result<()> {
    println!("Usage: boot_param_test <bin_file_with_boot_param> [--verbose]");
    err_str("Wrong CLI options")
}

fn verbose_dump<T: std::fmt::Debug + ?Sized>(verbose: bool, prefix: &str, obj: &T) {
    if verbose {
        println!("{prefix}: {obj:02x?}");
    }
}

trait CliArgs {
    fn check_and_remove_flag(&mut self, flag: &str) -> bool;
    fn check_and_remove_string(&mut self, optprefix: &str) -> Option<String>;
}

impl CliArgs for Vec<String> {
    fn check_and_remove_flag(&mut self, flag: &str) -> bool {
        let mut found = false;
        let mut i = 0;
        while let Some(elem) = self.get(i) {
            if elem == flag {
                self.remove(i);
                found = true;
            } else {
                i += 1;
            }
        }
        found
    }
    fn check_and_remove_string(&mut self, optprefix: &str) -> Option<String> {
        let mut res = None;
        let mut i = 0;
        while let Some(elem) = self.get(i) {
            if let Some(s) = elem.strip_prefix(optprefix) {
                res = Some(s.into());
                self.remove(i);
            } else {
                i += 1;
            }
        }
        res
    }
}

fn main() -> Result<()> {
    let mut args: Vec<String> = std::env::args().collect();
    let verbose = args.check_and_remove_flag("--verbose");
    let output_filename = args.check_and_remove_string("--output=");

    let input_filename = match &args[..] {
        [_, filename] => filename,
        _ => {
            return print_help();
        }
    };

    let boot_param = std::fs::read(input_filename)?;
    let boot_param = boot_param.as_slice();
    verbose_dump(verbose, "Input BootParam", boot_param);

    let dice_handover = extract_dice_handover(verbose, boot_param)?;
    let dice_handover = dice_handover.as_slice();
    verbose_dump(verbose, "Input handover", dice_handover);

    println!("--------------------------------");
    println!("Doing DICE handover parse");
    let parsed_dice = bcc_handover_parse(dice_handover)?;
    let cdi_attest = parsed_dice.cdi_attest();
    let dice_chain = parsed_dice.bcc().ok_or(anyhow!("No DICE chain"))?;
    verbose_dump(verbose, "CDI attest", cdi_attest);
    verbose_dump(verbose, "DICE chain", dice_chain);

    println!("--------------------------------");
    println!("Verifying original DICE with hwtrust");
    let mut options = Options::vsr17();
    options.verbose = verbose;
    options.allow_any_mode = true;
    options.rkp_instance = RkpInstance::Default;
    let session = Session { options };

    let chain = dice::ChainForm::from_cbor(&session, dice_chain)?;
    if let ChainForm::Degenerate(_) = chain {
        bail!("Degenerate DICE chain");
    }

    println!("--------------------------------");
    println!("Doing DICE chain structure check");
    let public_key = dice_chain_structure_check(dice_chain)?;

    println!("--------------------------------");
    println!("Checking DICE chain leaf subject public key");
    if verbose {
        println!(
            "DICE pubkey algorithm: {:?} = {:?}",
            public_key.cose_alg, public_key.alg
        );
        println!("DICE pubkey: {:?}", public_key.cose_key);
    }
    let public_key_bytes = check_public_key(&public_key)?;
    verbose_dump(verbose, "DICE pubkey bytes", public_key_bytes.as_slice());

    println!("--------------------------------");
    println!("Deriving CDI pubkey from CDI attest");
    let cdi_priv_key_seed = derive_cdi_private_key_seed(cdi_attest)?;
    verbose_dump(verbose, "CDI key seed", cdi_priv_key_seed.as_array());
    let dice_context = DiceContext {
        authority_algorithm: public_key.alg,
        subject_algorithm: public_key.alg
    };
    let (cdi_public_key, _) =
        keypair_from_seed(Some(dice_context), cdi_priv_key_seed.as_array())?;
    verbose_dump(verbose, "CDI pubkey", &cdi_public_key);
    if (public_key_bytes.as_slice() != cdi_public_key) {
        bail!("Derived CDI pubkey doesn't match DICE chain");
    }

    println!("--------------------------------");
    println!("Doing DICE extend");
    let mut next_handover_buf = [0u8; MAX_NEXT_HANDOVER_SIZE];
    let next_handover = dice_extend(dice_handover, &mut next_handover_buf, public_key.cose_alg)?;
    verbose_dump(verbose, "Next handover", next_handover);

    println!("--------------------------------");
    println!("Doing new DICE handover parse");
    let parsed_dice = bcc_handover_parse(next_handover)?;
    let cdi_attest = parsed_dice.cdi_attest();
    let dice_chain = parsed_dice.bcc().ok_or(anyhow!("No new DICE chain"))?;
    verbose_dump(verbose, "CDI attest", cdi_attest);
    verbose_dump(verbose, "New DICE chain", dice_chain);

    if let Some(name) = output_filename {
        println!("--------------------------------");
        println!("Saving new DICE in {name}");
        std::fs::write(name, dice_chain)?;
    }

    println!("--------------------------------");
    println!("Verifying new DICE with hwtrust");

    let chain = dice::ChainForm::from_cbor(&session, dice_chain)?;
    if let ChainForm::Degenerate(_) = chain {
        bail!("Degenerate new DICE chain");
    }

    println!("SUCCESS!");
    Ok(())
}

fn check_public_key(public_key: &PublicKey) -> Result<Vec<u8>> {
    if public_key.alg != KeyAlgorithm::EcdsaP256 {
        bail!("Unexpected DICE chain leaf subject public key algorithm");
    }

    let mut public_key_x = None;
    let mut public_key_y = None;
    for (label, value) in &public_key.cose_key.params {
        match label {
            Label::Int(-1) if value.as_integer() != Some(1.into()) => bail!("Unexpected curve"),
            Label::Int(-2) => public_key_x = Some(value.clone()),
            Label::Int(-3) => public_key_y = Some(value.clone()),
            _ => (),
        };
    }
    let public_key_x = public_key_x
        .map(Value::into_bytes)
        .map(Result::ok)
        .flatten()
        .ok_or(anyhow!(
            "Can't find X in DICE chain leaf subject public key"
        ))?;
    let public_key_y = public_key_y
        .map(Value::into_bytes)
        .map(Result::ok)
        .flatten()
        .ok_or(anyhow!(
            "Can't find Y in DICE chain leaf subject public key"
        ))?;
    let mut public_key_bytes: Vec<u8> = Vec::new();
    public_key_bytes.extend(public_key_x);
    public_key_bytes.extend(public_key_y);
    Ok(public_key_bytes)
}

fn dice_extend<'a, 'b>(
    dice_handover: &'a [u8],
    next_handover_buf: &'b mut [u8],
    cose_alg: iana::Algorithm,
) -> Result<&'b [u8]> {
    let code_hash: Hash = [0u8; HASH_SIZE];
    let auth_hash: Hash = [0u8; HASH_SIZE];
    let hidden: Hidden = [0u8; HIDDEN_SIZE];
    let mode: DiceMode = DiceMode::kDiceModeDebug;
    let config_descr = Value::Map(vec![
        (Value::from(-70002), Value::from("test")), // component name
        (Value::from(-70003), Value::from(1)),      // component version
        (Value::from(-70005), Value::from(1)),      // security version
    ]);
    let config = config_descr.to_vec()?;

    let dice_context = DiceContext {
        authority_algorithm: cose_alg.try_into()?,
        subject_algorithm: VM_KEY_ALGORITHM,
    };

    let dice_inputs = InputValues::new(
        code_hash,
        Config::Descriptor(&config),
        auth_hash,
        mode,
        hidden,
    );

    let next_handover_size =
        bcc_handover_main_flow(dice_handover, &dice_inputs, next_handover_buf, dice_context)?;

    if next_handover_size > MAX_NEXT_HANDOVER_SIZE {
        bail!("Too big next handover size");
    }

    Ok(&next_handover_buf[..next_handover_size])
}

fn extract_dice_handover(verbose: bool, boot_param_input: &[u8]) -> Result<Vec<u8>> {
    let boot_param: Value = ciborium::from_reader(boot_param_input)?;
    verbose_dump(verbose, "BootParam CBOR", &boot_param);

    // BootParam = {
    //     1  : uint,               ; structure version (0)
    //     2  : GSCBootParam,
    //     3  : AndroidDiceHandover,
    // }
    let boot_param = boot_param
        .as_map()
        .ok_or(anyhow!("BootParam is not a map"))?;

    if let Some(dice_handover) = value_for_key(boot_param, 3) {
        return cbor_util::serialize(dice_handover)
            .map_err(Error::new);
    }
    if let Some(dice_handover) = value_for_key(boot_param, 4) {
        return dice_handover
            .as_bytes()
            .map(Vec::clone)
            .ok_or(anyhow!("DICE handover is not BSTR"));
    }
    Err(anyhow!("BootParam doesn't contain DICE handover"))
}

fn dice_chain_structure_check(handover: &[u8]) -> Result<PublicKey> {
    // We don't attempt to fully validate the DICE chain (e.g. we don't check the signatures) -
    // we have to trust our loader. But if it's invalid CBOR or otherwise clearly ill-formed,
    // something is very wrong, so we fail.
    let handover_cbor = cbor_util::deserialize(handover)?;

    // Bcc = [
    //   PubKeyEd25519 / PubKeyECDSA256, // DK_pub
    //   + BccEntry,                     // Root -> leaf (KM_pub)
    // ]
    let dice_chain = match handover_cbor {
        Value::Array(v) if v.len() >= 2 => v,
        _ => bail!("Invalid top level Bcc format"),
    };

    // Decode all the DICE payloads to make sure they are well-formed.
    let payloads = dice_chain
        .into_iter()
        .skip(1)
        .map(get_bcc_entry_payload)
        .collect::<Result<Vec<_>>>()?;

    if !payloads.iter().all(check_payload_boot_mode) {
        bail!("Invalid boot mode specified");
    }
    // Safe to unwrap because we checked the length above.
    let last_payload = payloads.last().unwrap();
    check_payload_subject_public_key(last_payload)
}

type ValueMap = Vec<(Value, Value)>;
fn get_bcc_entry_payload(entry: Value) -> Result<ValueMap> {
    // BccEntry = [                                  // COSE_Sign1 (untagged)
    //     protected : bstr .cbor {
    //         1 : AlgorithmEdDSA / AlgorithmES256,  // Algorithm
    //     },
    //     unprotected: {},
    //     payload: bstr .cbor BccPayload,
    //     signature: bstr // PureEd25519(SigningKey, bstr .cbor BccEntryInput) /
    //                     // ECDSA(SigningKey, bstr .cbor BccEntryInput)
    // ]
    let entry = entry.into_array().or(err_str("BccEntry is not an array"))?;
    if entry.len() != 4 {
        bail!("BccEntry with a wrong number of elements");
    };
    let payload = entry[2]
        .as_bytes()
        .ok_or(anyhow!("BccEntryPayload is not bytes"))?;
    let payload: Value = cbor_util::deserialize(&payload)?;
    payload
        .into_map()
        .or(err_str("BccEntryPayload doesn't contain a map"))
}

fn value_for_key(payload: &ValueMap, key: i32) -> Option<&Value> {
    for (k, v) in payload {
        if k.as_integer() == Some(key.into()) {
            return Some(v);
        }
    }
    None
}

fn check_payload_boot_mode(payload: &ValueMap) -> bool {
    // BccPayload = {                     // CWT
    // ...
    //     ? -4670551 : bstr,             // Mode
    // ...
    // }
    match value_for_key(payload, -4670551) {
        None => true,
        Some(Value::Integer(_)) => true,
        Some(Value::Bytes(bytes)) if bytes.len() == 1 => true,
        _ => false,
    }
}

fn check_payload_subject_public_key(payload: &ValueMap) -> Result<PublicKey> {
    // BccPayload = {                             ; CWT
    // ...
    //   -4670552 : bstr .cbor PubKeyEd25519 /
    //              bstr .cbor PubKeyECDSA256 /
    //              bstr .cbor PubKeyECDSA384,    ; Subject Public Key
    // ...
    // }
    match value_for_key(payload, -4670552) {
        None => err_str("Subject public key missing"),
        Some(Value::Bytes(bytes)) => PublicKey::from_slice(bytes),
        _ => err_str("Subject public key not a byte string"),
    }
}

#[derive(Debug, Clone)]
struct PublicKey {
    pub cose_alg: iana::Algorithm,
    pub alg: KeyAlgorithm,
    pub cose_key: CoseKey,
}

impl PublicKey {
    fn from_slice(slice: &[u8]) -> Result<Self> {
        let cose_key = CoseKey::from_slice(slice)?;
        let Some(Algorithm::Assigned(cose_alg)) = cose_key.alg else {
            bail!("Invalid algorithm in public key");
        };
        let alg = KeyAlgorithm::try_from(cose_alg)?;
        Ok(Self {
            cose_alg,
            alg,
            cose_key,
        })
    }
}
