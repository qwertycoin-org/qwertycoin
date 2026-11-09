  void bind_endpoint_transport_v2(
      qwertycoin::epose::identity_descriptor_v2 &identity,
      const qwertycoin::epose::endpoint_descriptor_v2 &endpoint)
  {
    identity.version = endpoint.version;
    identity.endpoint_transport = endpoint.version == 2
        ? static_cast<uint8_t>(endpoint.transport) : 1;
    identity.dial_capabilities = endpoint.dial_capabilities;
    identity.dns_address_families = endpoint.dns_address_families;
  }
