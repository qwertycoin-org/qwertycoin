import unittest

from check_epose_probe_connector_wiring import check_probe


FACTORY = "auto connector = qwertycoin::epose::service_probe_connector_v2(endpoint, capabilities);\n"
BINDING = "client.set_connector(std::move(connector));\n"


class ProbeConnectorWiringTest(unittest.TestCase):
    def test_live_binding(self):
        check_probe(FACTORY + BINDING)

    def test_binding_after_label(self):
        check_probe(FACTORY + "setup: " + BINDING)

    def test_inline_binding_required(self):
        check_probe(FACTORY + BINDING)
        with self.assertRaisesRegex(ValueError, "not installed"):
            check_probe(FACTORY)

    def test_include_does_not_substitute_for_inline_wiring(self):
        with self.assertRaisesRegex(ValueError, "preprocessor directive"):
            check_probe('#include "probe_connector.h"\n' + FACTORY + BINDING)

    def test_other_client_binding_does_not_bind_probe_client(self):
        for other_client in ("otherclient", "holder.client", "holder->client"):
            with self.subTest(other_client=other_client):
                with self.assertRaisesRegex(ValueError, "not installed"):
                    check_probe(FACTORY + BINDING.replace("client.", f"{other_client}."))

    def test_spaced_member_access_does_not_bind_probe_client(self):
        for other_client in ("holder . client", "holder -> client", "holder :: client",
                             "holder /* gap */ . /* gap */ client"):
            with self.subTest(other_client=other_client):
                with self.assertRaisesRegex(ValueError, "not installed"):
                    check_probe(FACTORY + BINDING.replace("client.", f"{other_client}."))

    def test_spliced_member_access_does_not_bind_probe_client(self):
        for other_client in ("holder . \\\n client", "holder -\\\n> client", "holder :\\\n: client"):
            with self.subTest(other_client=other_client):
                with self.assertRaisesRegex(ValueError, "not installed"):
                    check_probe(FACTORY + BINDING.replace("client.", f"{other_client}."))

    def test_spliced_local_client_binding(self):
        check_probe(FACTORY + BINDING.replace("client.", "cli\\\nent."))

    def test_spliced_factory(self):
        check_probe(FACTORY.replace("connector =", "con\\\nnector =", 1) + BINDING)

    def test_spliced_raw_string_does_not_supply_binding(self):
        for opener in ('R"tag(', 'R\\\n"tag('):
            with self.subTest(opener=opener):
                probe = FACTORY + 'auto unused = ' + opener + 'payload )ta\\\ng" ' + BINDING + ' )tag";\n'
                with self.assertRaisesRegex(ValueError, "not installed"):
                    check_probe(probe)

    def test_spliced_raw_string_does_not_supply_factory(self):
        probe = 'auto unused = R"tag(payload )ta\\\ng" ' + FACTORY + ' )tag";\n' + BINDING
        with self.assertRaisesRegex(ValueError, "factory is missing"):
            check_probe(probe)

    def test_spliced_raw_string_in_inline_region_does_not_supply_binding(self):
        with self.assertRaisesRegex(ValueError, "not installed"):
            check_probe(FACTORY + 'auto unused = R"tag(payload )ta\\\ng" ' + BINDING + ' )tag";\n')

    def test_commented_factory_does_not_supply_wiring(self):
        with self.assertRaisesRegex(ValueError, "factory is missing"):
            check_probe('// ' + FACTORY + BINDING)

    def test_duplicate_inline_factories(self):
        with self.assertRaisesRegex(ValueError, "duplicated"):
            check_probe(FACTORY + FACTORY + BINDING)

    def test_raw_string_factory_does_not_supply_wiring(self):
        with self.assertRaisesRegex(ValueError, "factory is missing"):
            check_probe('R"tag(\n' + FACTORY + ')tag";\n' + BINDING)

    def test_line_commented_binding(self):
        with self.assertRaisesRegex(ValueError, "not installed"):
            check_probe(FACTORY + "// " + BINDING)

    def test_block_commented_binding(self):
        with self.assertRaisesRegex(ValueError, "not installed"):
            check_probe(FACTORY + "/* " + BINDING + " */")

    def test_string_literal_binding(self):
        with self.assertRaisesRegex(ValueError, "not installed"):
            check_probe(FACTORY + 'const char *unused = "client.set_connector(std::move(connector))";\n')

    def test_raw_string_binding(self):
        with self.assertRaisesRegex(ValueError, "not installed"):
            check_probe(FACTORY + 'const char *unused = R"tag(\n' + BINDING + ')tag";\n')

    def test_string_literal_factory(self):
        with self.assertRaisesRegex(ValueError, "factory is missing"):
            check_probe('const char *unused = "auto connector = qwertycoin::epose::service_probe_connector_v2(endpoint, capabilities)";\n' + BINDING)

    def test_url_before_live_binding(self):
        check_probe(FACTORY + 'const char *url = "https://example"; ' + BINDING)

    def test_spliced_string_binding(self):
        with self.assertRaisesRegex(ValueError, "not installed"):
            check_probe(FACTORY + 'const char *unused = "prefix\\\nclient.set_connector(std::move(connector))";\n')

    def test_spliced_line_comment_binding(self):
        with self.assertRaisesRegex(ValueError, "not installed"):
            check_probe(FACTORY + '// ignored \\\n' + BINDING)

    def test_spaced_spliced_line_comment_binding(self):
        for whitespace in (' ', '\t', '\v', '\f'):
            with self.subTest(whitespace=whitespace):
                with self.assertRaisesRegex(ValueError, "not installed"):
                    check_probe(FACTORY + '// ignored \\' + whitespace + '\n' + BINDING)

    def test_disabled_binding(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if 0\n' + BINDING + '#endif\n')

    def test_macro_binding(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#define UNUSED ' + BINDING)

    def test_continued_macro_binding(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#define UNUSED \\\n' + BINDING)

    def test_binding_after_disabled_block(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if 0\nignored();\n#endif\n' + BINDING)

    def test_binding_in_else_branch(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if 0\nignored();\n#else\n' + BINDING + '#endif\n')

    def test_binding_in_true_branch(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if 1\n' + BINDING + '#endif\n')

    def test_binding_in_named_conditional_branch(self):
        for directive in ("ifdef", "ifndef"):
            with self.subTest(directive=directive):
                with self.assertRaisesRegex(ValueError, "directive"):
                    check_probe(FACTORY + f'#{directive} SOME_FLAG\n' + BINDING + '#endif\n')

    def test_binding_under_vertical_and_form_feed_directives(self):
        for whitespace in ("\v", "\f"):
            for directive in (f'#{whitespace}if 0\n', f'{whitespace}#if 0\n'):
                with self.subTest(directive=directive):
                    with self.assertRaisesRegex(ValueError, "directive"):
                        check_probe(FACTORY + directive + BINDING + '#endif\n')

    def test_binding_in_else_after_negated_zero(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if !0\nignored();\n#else\n' + BINDING + '#endif\n')

    def test_factory_in_true_branch(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe('#if 1\n' + FACTORY + '#endif\n' + BINDING)

    def test_binding_in_dead_else_branch(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if 1\nignored();\n#else\n' + BINDING + '#endif\n')

    def test_binding_in_nested_disabled_branch(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if 0\n#if 1\n' + BINDING + '#endif\n#endif\n')

    def test_binding_in_continued_disabled_branch(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if \\\n0\n' + BINDING + '#endif\n')

    def test_binding_in_continued_macro_string(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#define UNUSED "text\\\n" ' + BINDING)

    def test_binding_after_continued_live_branch(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if \\\n1\n' + BINDING + '#endif\n')

    def test_binding_in_zero_elif_after_unknown_condition(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if SOME_FLAG\nignored();\n#elif 0\n' + BINDING + '#endif\n')

    def test_binding_in_else_after_true_elif(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if SOME_FLAG\nignored();\n#elif 1\nignored();\n#else\n' + BINDING + '#endif\n')

    def test_binding_in_parenthesized_zero_branch(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if ((0))\n' + BINDING + '#endif\n')

    def test_binding_in_false_conjunction(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if 0 && SOME_FLAG\n' + BINDING + '#endif\n')

    def test_binding_in_false_conjunction_on_right(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if SOME_FLAG && 0\n' + BINDING + '#endif\n')

    def test_binding_in_else_after_true_disjunction(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if SOME_FLAG || 1\nignored();\n#else\n' + BINDING + '#endif\n')

    def test_binding_in_else_after_nonzero_decimal(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if 2 || SOME_FLAG\nignored();\n#else\n' + BINDING + '#endif\n')

    def test_binding_in_zero_with_unsigned_long_suffix(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if 0UL && SOME_FLAG\n' + BINDING + '#endif\n')

    def test_binding_in_else_after_nonzero_octal(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if 01 || SOME_FLAG\nignored();\n#else\n' + BINDING + '#endif\n')

    def test_binding_in_zero_hex_with_unsigned_long_long_suffix(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if 0x0ULL && SOME_FLAG\n' + BINDING + '#endif\n')

    def test_binding_in_else_after_nonzero_binary_with_unsigned_long_long_suffix(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if 0b1ULL || SOME_FLAG\nignored();\n#else\n' + BINDING + '#endif\n')

    def test_binding_in_zero_binary_with_unsigned_suffix(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if 0B0u && SOME_FLAG\n' + BINDING + '#endif\n')

    def test_binding_in_else_after_negative_constant(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if -2 || SOME_FLAG\nignored();\n#else\n' + BINDING + '#endif\n')

    def test_binding_in_positive_zero(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + '#if +0 && SOME_FLAG\n' + BINDING + '#endif\n')

    def test_binding_in_else_after_digit_separated_binary(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + "#if 0b1'0ULL || SOME_FLAG\nignored();\n#else\n" + BINDING + '#endif\n')

    def test_binding_in_zero_digit_separated_decimal(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + "#if 0'0UL && SOME_FLAG\n" + BINDING + '#endif\n')

    def test_binding_in_else_after_multiple_digit_separators(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + "#if 1'0'0 || SOME_FLAG\nignored();\n#else\n" + BINDING + '#endif\n')

    def test_binding_in_else_after_digit_separated_hex_and_octal(self):
        for literal in ("0xA'0ULL", "0'7u"):
            with self.subTest(literal=literal):
                with self.assertRaisesRegex(ValueError, "directive"):
                    check_probe(FACTORY + f'#if {literal} || SOME_FLAG\nignored();\n#else\n' + BINDING + '#endif\n')

    def test_spaced_macro_continuation_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + r'#define UNUSED \ ' + '\n' + BINDING)

    def test_hash_in_comment_and_literal_is_ignored(self):
        check_probe(FACTORY + '/* #if 0 */\n' + 'const char *text = "#if 0";\n' + BINDING)

    def test_empty_directive_after_binding_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "directive"):
            check_probe(FACTORY + BINDING + '#\n')

    def test_commented_factory(self):
        with self.assertRaisesRegex(ValueError, "factory is missing"):
            check_probe("// " + FACTORY + BINDING)


if __name__ == "__main__":
    unittest.main()
