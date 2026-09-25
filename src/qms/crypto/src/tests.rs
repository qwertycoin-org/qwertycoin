use super::*;

fn commit_package(
    engine: Engine,
    genesis: [u8; GENESIS_BYTES],
) -> (Engine, Vec<u8>, [u8; INVITATION_ID_BYTES]) {
    let prepared = engine.prepare_contact_package(genesis).unwrap();
    let package = ContactPackage::decode(&prepared.package).unwrap();
    (
        Engine::from_state(&prepared.next_state).unwrap(),
        prepared.package,
        package.invitation_id,
    )
}

fn import(
    engine: Engine,
    local_invitation: [u8; INVITATION_ID_BYTES],
    remote: &[u8],
) -> (Engine, String) {
    let prepared = engine
        .prepare_import_contact(local_invitation, remote, 1_700_000_000)
        .unwrap();
    (
        Engine::from_state(&prepared.next_state).unwrap(),
        prepared.contact_id,
    )
}

#[test]
fn pqxdh_triple_ratchet_roundtrip_and_state_transaction() {
    let genesis = [0x42; GENESIS_BYTES];
    let (alice, alice_package, alice_invitation) = commit_package(Engine::new().unwrap(), genesis);
    let (bob, bob_package, bob_invitation) = commit_package(Engine::new().unwrap(), genesis);
    let (alice, bob_contact) = import(alice, alice_invitation, &bob_package);
    let (bob, alice_contact) = import(bob, bob_invitation, &alice_package);

    let alice_outgoing = alice.transport_context(&bob_contact, true).unwrap();
    let bob_incoming = bob.transport_context(&alice_contact, false).unwrap();
    assert_eq!(alice_outgoing, bob_incoming);
    assert_ne!(
        alice_outgoing,
        alice.transport_context(&bob_contact, false).unwrap()
    );

    let before_send = alice.state().unwrap();
    let prepared = alice
        .prepare_send_text(&bob_contact, &"x".repeat(MAX_TEXT_BYTES), 1_700_000_001)
        .unwrap();
    assert_eq!(prepared.ciphertext.message_type, 1);
    assert!(prepared.ciphertext.bytes.len() + 1 <= 9556);
    assert_eq!(
        alice.state().unwrap(),
        before_send,
        "prepare must not mutate committed state"
    );
    let alice = Engine::from_state(&prepared.next_state).unwrap();

    let before_receive = bob.state().unwrap();
    let received = bob
        .prepare_receive_text(&alice_contact, &prepared.ciphertext)
        .unwrap();
    assert_eq!(received.text, "x".repeat(MAX_TEXT_BYTES));
    assert_eq!(received.message_id, prepared.message_id);
    assert_eq!(
        bob.state().unwrap(),
        before_receive,
        "receive prepare must be transactional"
    );
    let bob = Engine::from_state(&received.next_state).unwrap();

    assert!(matches!(
        bob.prepare_receive_text(&alice_contact, &prepared.ciphertext),
        Err(Error::Signal(_)) | Err(Error::InvalidMessage("replayed message id"))
    ));

    let reply = bob
        .prepare_send_text(&alice_contact, "reply", 1_700_000_002)
        .unwrap();
    assert_eq!(reply.ciphertext.message_type, 2);
    let bob = Engine::from_state(&reply.next_state).unwrap();
    let opened = alice
        .prepare_receive_text(&bob_contact, &reply.ciphertext)
        .unwrap();
    assert_eq!(opened.text, "reply");
    assert!(!bob.state().unwrap().is_empty());
}

#[test]
fn ongoing_ratchet_accepts_out_of_order_and_rejects_duplicate_and_tamper() {
    let genesis = [0x51; GENESIS_BYTES];
    let (alice, alice_package, alice_invitation) = commit_package(Engine::new().unwrap(), genesis);
    let (bob, bob_package, bob_invitation) = commit_package(Engine::new().unwrap(), genesis);
    let (mut alice, bob_contact) = import(alice, alice_invitation, &bob_package);
    let (mut bob, alice_contact) = import(bob, bob_invitation, &alice_package);

    let bootstrap = alice
        .prepare_send_text(&bob_contact, "bootstrap", 1_700_000_090)
        .unwrap();
    assert_eq!(1, bootstrap.ciphertext.message_type);
    alice = Engine::from_state(&bootstrap.next_state).unwrap();
    let opened = bob
        .prepare_receive_text(&alice_contact, &bootstrap.ciphertext)
        .unwrap();
    bob = Engine::from_state(&opened.next_state).unwrap();
    let reply = bob
        .prepare_send_text(&alice_contact, "bootstrap-reply", 1_700_000_091)
        .unwrap();
    bob = Engine::from_state(&reply.next_state).unwrap();
    let opened = alice
        .prepare_receive_text(&bob_contact, &reply.ciphertext)
        .unwrap();
    alice = Engine::from_state(&opened.next_state).unwrap();

    let mut messages = Vec::new();
    for index in 0..4 {
        let prepared = alice
            .prepare_send_text(
                &bob_contact,
                &format!("message-{index}"),
                1_700_000_100 + index,
            )
            .unwrap();
        alice = Engine::from_state(&prepared.next_state).unwrap();
        messages.push(prepared);
    }
    assert!(
        messages
            .iter()
            .all(|message| message.ciphertext.message_type == 2)
    );

    for index in [3usize, 0, 2, 1] {
        let received = bob
            .prepare_receive_text(&alice_contact, &messages[index].ciphertext)
            .unwrap();
        assert_eq!(format!("message-{index}"), received.text);
        bob = Engine::from_state(&received.next_state).unwrap();
    }
    assert!(
        bob.prepare_receive_text(&alice_contact, &messages[1].ciphertext)
            .is_err()
    );

    let mut tampered = alice
        .prepare_send_text(&bob_contact, "tamper", 1_700_000_200)
        .unwrap()
        .ciphertext;
    let last = tampered.bytes.len() - 1;
    tampered.bytes[last] ^= 1;
    assert!(bob.prepare_receive_text(&alice_contact, &tampered).is_err());
}

#[test]
fn outer_secret_offer_ack_grace_and_bounded_retirement() {
    let genesis = [0x73; GENESIS_BYTES];
    let (alice, alice_package, alice_invitation) = commit_package(Engine::new().unwrap(), genesis);
    let (bob, bob_package, bob_invitation) = commit_package(Engine::new().unwrap(), genesis);
    let (mut alice, bob_contact) = import(alice, alice_invitation, &bob_package);
    let (mut bob, alice_contact) = import(bob, bob_invitation, &alice_package);

    let initial_alice_incoming = alice.transport_contexts(&bob_contact, false).unwrap()[0].clone();

    for index in 0..OUTER_ROTATION_INTERVAL {
        let sent = alice
            .prepare_send_text(
                &bob_contact,
                &format!("warmup-{index}"),
                1_700_001_000 + index,
            )
            .unwrap();
        alice = Engine::from_state(&sent.next_state).unwrap();
        let received = bob
            .prepare_receive_text(&alice_contact, &sent.ciphertext)
            .unwrap();
        bob = Engine::from_state(&received.next_state).unwrap();
    }

    // The first offer is committed locally but its carrier is lost. A later
    // message must repeat the same authenticated offer rather than generating
    // another secret or requiring a control-only transaction.
    let lost = alice
        .prepare_send_text(&bob_contact, "lost-offer", 1_700_002_000)
        .unwrap();
    alice = Engine::from_state(&lost.next_state).unwrap();
    assert_eq!(
        2,
        alice.transport_contexts(&bob_contact, false).unwrap().len()
    );

    let repeated = alice
        .prepare_send_text(&bob_contact, "repeated-offer", 1_700_002_001)
        .unwrap();
    alice = Engine::from_state(&repeated.next_state).unwrap();
    let received = bob
        .prepare_receive_text(&alice_contact, &repeated.ciphertext)
        .unwrap();
    bob = Engine::from_state(&received.next_state).unwrap();

    let bob_new_outgoing = bob.transport_context(&alice_contact, true).unwrap();
    let alice_accepts = alice.transport_contexts(&bob_contact, false).unwrap();
    assert!(alice_accepts.contains(&bob_new_outgoing));
    assert_ne!(
        initial_alice_incoming.root_secret,
        bob_new_outgoing.root_secret
    );

    let acknowledgement = bob
        .prepare_send_text(&alice_contact, "ack", 1_700_002_002)
        .unwrap();
    bob = Engine::from_state(&acknowledgement.next_state).unwrap();
    let received = alice
        .prepare_receive_text(&bob_contact, &acknowledgement.ciphertext)
        .unwrap();
    alice = Engine::from_state(&received.next_state).unwrap();
    let after_first_ack = alice.transport_contexts(&bob_contact, false).unwrap();
    assert_eq!(2, after_first_ack.len());
    assert!(after_first_ack.contains(&bob_new_outgoing));
    assert!(after_first_ack.contains(&initial_alice_incoming));

    // Advance to and complete a second rotation. The oldest retiring secret
    // must then be deleted; the incoming set remains bounded to active plus
    // one grace-period secret.
    while alice
        .state
        .contacts
        .get(&bob_contact)
        .unwrap()
        .local_outer
        .send_count
        < OUTER_ROTATION_INTERVAL * 2
    {
        let sent = alice
            .prepare_send_text(&bob_contact, "advance", 1_700_003_000)
            .unwrap();
        alice = Engine::from_state(&sent.next_state).unwrap();
        let received = bob
            .prepare_receive_text(&alice_contact, &sent.ciphertext)
            .unwrap();
        bob = Engine::from_state(&received.next_state).unwrap();
    }
    let second_offer = alice
        .prepare_send_text(&bob_contact, "second-offer", 1_700_003_001)
        .unwrap();
    alice = Engine::from_state(&second_offer.next_state).unwrap();
    let received = bob
        .prepare_receive_text(&alice_contact, &second_offer.ciphertext)
        .unwrap();
    bob = Engine::from_state(&received.next_state).unwrap();
    let second_ack = bob
        .prepare_send_text(&alice_contact, "second-ack", 1_700_003_002)
        .unwrap();
    let received = alice
        .prepare_receive_text(&bob_contact, &second_ack.ciphertext)
        .unwrap();
    alice = Engine::from_state(&received.next_state).unwrap();
    let after_second_ack = alice.transport_contexts(&bob_contact, false).unwrap();
    assert_eq!(2, after_second_ack.len());
    assert!(!after_second_ack.contains(&initial_alice_incoming));
}

#[test]
fn rejects_oversize_text_and_tampered_package() {
    let genesis = [0x24; GENESIS_BYTES];
    let (engine, package, invitation) = commit_package(Engine::new().unwrap(), genesis);
    let other = Engine::new().unwrap();
    let (other, other_package, _) = commit_package(other, genesis);
    let (engine, contact) = import(engine, invitation, &other_package);
    assert!(matches!(
        engine.prepare_send_text(&contact, &"z".repeat(MAX_TEXT_BYTES + 1), 1),
        Err(Error::InvalidMessage(_))
    ));

    let mut tampered = package;
    let index = tampered.len() / 2;
    tampered[index] ^= 1;
    assert!(ContactPackage::decode(&tampered).is_err());
    assert!(!other.state().unwrap().is_empty());
}

#[test]
fn contact_import_is_idempotent_and_distinct_contacts_remain_separate() {
    let genesis = [0x11; GENESIS_BYTES];
    let (alice, alice_package, alice_invitation) = commit_package(Engine::new().unwrap(), genesis);
    let (_, bob_package, _) = commit_package(Engine::new().unwrap(), genesis);
    let first = alice
        .prepare_import_contact(alice_invitation, &bob_package, 1)
        .unwrap();
    let alice = Engine::from_state(&first.next_state).unwrap();
    let second = alice
        .prepare_import_contact(alice_invitation, &bob_package, 2)
        .unwrap();
    assert_eq!(first.contact_id, second.contact_id);

    let (_, third_package, _) = commit_package(Engine::new().unwrap(), genesis);
    assert!(
        alice
            .prepare_import_contact(alice_invitation, &third_package, 3)
            .is_ok()
    );
    assert!(!alice_package.is_empty());
}
