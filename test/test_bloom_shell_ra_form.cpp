#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "../src/bloomShell/bloom_shell_ra_form.h"
}

TEST(BloomShellRaFormTest, EditsBothBoundedFieldsAndMasksTheToken)
{
    BloomShellRaForm form{};
    bloom_shell_ra_form_init(&form);
    ASSERT_EQ(0, bloom_shell_ra_form_append(&form));
    EXPECT_STREQ("1", form.username);
    bloom_shell_ra_form_toggle_field(&form);
    ASSERT_EQ(0, bloom_shell_ra_form_append(&form));
    EXPECT_STREQ("1", form.token);
    char label[96]{};
    ASSERT_EQ(0, bloom_shell_ra_form_label(&form, label, sizeof(label)));
    EXPECT_STREQ("Token: *", label);
    EXPECT_EQ(nullptr, strstr(label, form.token));
    ASSERT_EQ(0, bloom_shell_ra_form_backspace(&form));
    EXPECT_STREQ("", form.token);
    bloom_shell_ra_form_toggle_field(&form);
    ASSERT_EQ(0, bloom_shell_ra_form_backspace(&form));
    EXPECT_STREQ("", form.username);
}

TEST(BloomShellRaFormTest, UsesTheSharedKeyboardModel)
{
    BloomShellRaForm form{};
    bloom_shell_ra_form_init(&form);
    ASSERT_EQ(1, bloom_shell_ra_form_move(&form, 0, 1));
    ASSERT_EQ(0, bloom_shell_ra_form_append(&form));
    EXPECT_STREQ("q", form.username);
    bloom_shell_ra_form_cycle_mode(&form);
    ASSERT_EQ(0, bloom_shell_ra_form_append(&form));
    EXPECT_STREQ("qQ", form.username);
}

TEST(BloomShellRaFormTest, SubmitsFixedArgumentsViaStdinAndClearsTheSecret)
{
    const auto directory = std::filesystem::temp_directory_path() /
                           ("bloom-ra-form-" + std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    const auto script = directory / "bloom-ra";
    const auto arguments = directory / "arguments";
    const auto input = directory / "input";
    {
        std::ofstream output(script);
        output << "#!/bin/sh\n"
                  "printf '%s\\n' \"$*\" >\"$(dirname \"$0\")/arguments\"\n"
                  "IFS= read -r token\n"
                  "printf '%s' \"$token\" >\"$(dirname \"$0\")/input\"\n"
                  "[ \"$#\" -eq 5 ] && [ \"$1\" = account ] && [ \"$2\" = configure ]\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    BloomShellRaForm form{};
    bloom_shell_ra_form_init(&form);
    snprintf(form.username, sizeof(form.username), "%s", "Bloom User");
    snprintf(form.token, sizeof(form.token), "%s", "private token");
    ASSERT_EQ(0, bloom_shell_ra_form_submit(script.c_str(), &form));
    EXPECT_STREQ("", form.token);
    std::ifstream argument_input(arguments);
    std::string argument_value;
    std::getline(argument_input, argument_value);
    EXPECT_EQ("account configure Bloom User softcore automatic", argument_value);
    std::ifstream token_input(input);
    std::string token_value;
    std::getline(token_input, token_value);
    EXPECT_EQ("private token", token_value);
    std::filesystem::remove_all(directory);
}

TEST(BloomShellRaFormTest, RejectsIncompleteOrRelativeSubmissions)
{
    BloomShellRaForm form{};
    bloom_shell_ra_form_init(&form);
    EXPECT_NE(0, bloom_shell_ra_form_submit("bloom-ra", &form));
    snprintf(form.username, sizeof(form.username), "%s", "user");
    snprintf(form.token, sizeof(form.token), "%s", "token");
    EXPECT_NE(0, bloom_shell_ra_form_submit("bloom-ra", &form));
    EXPECT_STREQ("", form.token);
    bloom_shell_ra_form_clear(&form);
    EXPECT_STREQ("", form.username);
    EXPECT_STREQ("", form.token);
}

TEST(BloomShellRaFormTest, SignsOutWithFixedArgumentsWithoutAShell)
{
    const auto directory = std::filesystem::temp_directory_path() /
                           ("bloom-ra-sign-out-" + std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    const auto script = directory / "bloom-ra";
    const auto arguments = directory / "arguments";
    {
        std::ofstream output(script);
        output << "#!/bin/sh\n"
                  "printf '%s\\n' \"$*\" >\"$(dirname \"$0\")/arguments\"\n"
                  "[ \"$#\" -eq 2 ] && [ \"$1\" = account ] && [ \"$2\" = sign-out ]\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    ASSERT_EQ(0, bloom_shell_ra_sign_out(script.c_str()));
    std::ifstream input(arguments);
    std::string value;
    std::getline(input, value);
    EXPECT_EQ("account sign-out", value);
    EXPECT_NE(0, bloom_shell_ra_sign_out("bloom-ra"));
    std::filesystem::remove_all(directory);
}
