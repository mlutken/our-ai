#!/usr/bin/python

import torch
import torch.nn as nn
from transformers import AutoModelForCausalLM, AutoTokenizer
from typing import List, Optional, Tuple

class EagleSpeculativeDecoder:
    def __init__(
        self,
        main_model_name: str = "mistralai/Mistral-7B-v0.1",
        draft_model_name: str = "TinyLlama/TinyLlama-1.1B",
        max_draft_length: int = 5,
        device: str = "cuda" if torch.cuda.is_available() else "cpu"
    ):
        """
        Initialize EAGLE speculative decoder.

        Args:
            main_model_name: Name/path of the main (larger) model
            draft_model_name: Name/path of the draft (smaller) model
            max_draft_length: Maximum number of tokens to draft at once
            device: Device to run models on
        """
        self.device = device
        self.max_draft_length = max_draft_length

        # Load models
        print("Loading main model...")
        self.main_model = AutoModelForCausalLM.from_pretrained(
            main_model_name,
            torch_dtype=torch.float16 if "cuda" in device else torch.float32,
            device_map="auto" if "cuda" in device else None
        ).eval()

        print("Loading draft model...")
        self.draft_model = AutoModelForCausalLM.from_pretrained(
            draft_model_name,
            torch_dtype=torch.float16 if "cuda" in device else torch.float32,
            device_map="auto" if "cuda" in device else None
        ).eval()

        # Tokenizer
        self.tokenizer = AutoTokenizer.from_pretrained(main_model_name)

        # Warmup
        self._warmup()

    def _warmup(self):
        """Warm up models to avoid first-run overhead."""
        print("Warming up models...")
        input_ids = torch.randint(0, 100, (1, 10), device=self.device)
        with torch.no_grad():
            _ = self.main_model(input_ids)
            _ = self.draft_model(input_ids)

    def _speculative_generate(
        self,
        input_ids: torch.Tensor,
        max_new_tokens: int,
        temperature: float = 0.7,
        top_k: Optional[int] = None,
        top_p: Optional[float] = None
    ) -> torch.Tensor:
        """
        Generate tokens using EAGLE speculative decoding.

        Args:
            input_ids: Input token IDs
            max_new_tokens: Maximum number of new tokens to generate
            temperature: Sampling temperature
            top_k: Top-k sampling
            top_p: Top-p (nucleus) sampling

        Returns:
            Generated token IDs
        """
        generated_tokens = []
        current_input = input_ids.clone()

        for _ in range(max_new_tokens):
            # Step 1: Draft multiple tokens
            draft_tokens = self._draft_tokens(current_input, temperature, top_k, top_p)

            # Step 2: Verify draft tokens
            accepted_tokens, new_input = self._verify_drafts(
                current_input, draft_tokens, temperature, top_k, top_p
            )

            # Update generated tokens and input
            generated_tokens.extend(accepted_tokens.tolist()[0])
            current_input = new_input

            # Early stopping if EOS token is generated
            if (accepted_tokens == self.tokenizer.eos_token_id).any():
                break

        return torch.cat([input_ids, torch.tensor(generated_tokens, device=self.device)], dim=1)

    def _draft_tokens(
        self,
        input_ids: torch.Tensor,
        temperature: float,
        top_k: Optional[int],
        top_p: Optional[float]
    ) -> torch.Tensor:
        """
        Generate draft tokens using the draft model.

        Args:
            input_ids: Current input token IDs
            temperature: Sampling temperature
            top_k: Top-k sampling
            top_p: Top-p sampling

        Returns:
            Draft token IDs (shape: [1, draft_length])
        """
        draft_tokens = []
        current_ids = input_ids.clone()

        for _ in range(self.max_draft_length):
            with torch.no_grad():
                outputs = self.draft_model(
                    current_ids,
                    use_cache=True
                )
                next_token_logits = outputs.logits[:, -1, :]

                # Apply sampling
                next_token = self._sample_token(
                    next_token_logits,
                    temperature=temperature,
                    top_k=top_k,
                    top_p=top_p
                )

            draft_tokens.append(next_token)
            current_ids = torch.cat([current_ids, next_token.unsqueeze(1)], dim=1)

        return torch.cat(draft_tokens, dim=1)

    def _verify_drafts(
        self,
        input_ids: torch.Tensor,
        draft_tokens: torch.Tensor,
        temperature: float,
        top_k: Optional[int],
        top_p: Optional[float]
    ) -> Tuple[torch.Tensor, torch.Tensor]:
        """
        Verify draft tokens using the main model.

        Args:
            input_ids: Original input token IDs
            draft_tokens: Draft token IDs to verify
            temperature: Sampling temperature
            top_k: Top-k sampling
            top_p: Top-p sampling

        Returns:
            Tuple of (accepted_tokens, new_input_ids)
        """
        # Prepare input with draft tokens
        draft_length = draft_tokens.size(1)
        extended_input = torch.cat([input_ids, draft_tokens], dim=1)

        with torch.no_grad():
            outputs = self.main_model(
                extended_input,
                use_cache=True
            )
            logits = outputs.logits[:, -draft_length:, :]

        # Check acceptance of each draft token
        accepted_tokens = []
        current_input = input_ids.clone()

        for i in range(draft_length):
            next_token_logits = logits[:, i, :]

            # Sample from main model
            sampled_token = self._sample_token(
                next_token_logits,
                temperature=temperature,
                top_k=top_k,
                top_p=top_p
            )

            # Check if draft token matches sampled token
            draft_token = draft_tokens[:, i]

            if draft_token.item() == sampled_token.item():
                accepted_tokens.append(draft_token)
                current_input = torch.cat([current_input, draft_token.unsqueeze(1)], dim=1)
            else:
                # Fall back to standard decoding
                with torch.no_grad():
                    fallback_output = self.main_model(
                        current_input,
                        use_cache=True
                    )
                    fallback_logits = fallback_output.logits[:, -1, :]
                    fallback_token = self._sample_token(
                        fallback_logits,
                        temperature=temperature,
                        top_k=top_k,
                        top_p=top_p
                    )
                accepted_tokens.append(fallback_token)
                current_input = torch.cat([current_input, fallback_token.unsqueeze(1)], dim=1)
                break

        return torch.tensor([accepted_tokens], device=self.device), current_input

    def _sample_token(
        self,
        logits: torch.Tensor,
        temperature: float,
        top_k: Optional[int],
        top_p: Optional[float]
    ) -> torch.Tensor:
        """
        Sample a token from logits using temperature, top-k, and top-p sampling.

        Args:
            logits: Model logits
            temperature: Sampling temperature
            top_k: Top-k sampling
            top_p: Top-p sampling

        Returns:
            Sampled token ID
        """
        if temperature > 0:
            logits = logits / temperature

        if top_k is not None:
            v, _ = torch.topk(logits, top_k)
            logits[logits < v[:, [-1]]] = float('-inf')

        if top_p is not None:
            sorted_logits, sorted_indices = torch.sort(logits, descending=True)
            cum_probs = torch.cumsum(torch.softmax(sorted_logits, dim=-1), dim=-1)
            sorted_indices_to_remove = cum_probs > top_p
            sorted_indices_to_remove[..., 1:] = sorted_indices_to_remove[..., :-1].clone()
            sorted_indices_to_remove[..., 0] = 0
            logits[sorted_indices] = sorted_indices_to_remove.float().masked_fill_(
                sorted_indices_to_remove, float('-inf')
            )

        probs = torch.softmax(logits, dim=-1)
        return torch.multinomial(probs, num_samples=1)

    def generate(
        self,
        prompt: str,
        max_new_tokens: int = 50,
        temperature: float = 0.7,
        top_k: Optional[int] = None,
        top_p: Optional[float] = None
    ) -> str:
        """
        Generate text using EAGLE speculative decoding.

        Args:
            prompt: Input prompt
            max_new_tokens: Maximum number of new tokens to generate
            temperature: Sampling temperature
            top_k: Top-k sampling
            top_p: Top-p sampling

        Returns:
            Generated text
        """
        # Tokenize input
        input_ids = self.tokenizer(prompt, return_tensors="pt").input_ids.to(self.device)

        # Generate tokens
        output_ids = self._speculative_generate(
            input_ids,
            max_new_tokens=max_new_tokens,
            temperature=temperature,
            top_k=top_k,
            top_p=top_p
        )

        # Decode output
        return self.tokenizer.decode(output_ids[0], skip_special_tokens=True)

# Example usage
if __name__ == "__main__":
    # Initialize EAGLE decoder
    eagle = EagleSpeculativeDecoder(
        main_model_name="mistralai/Mistral-7B-v0.1",
        draft_model_name="TinyLlama/TinyLlama-1.1B",
        max_draft_length=5,
        device="cuda" if torch.cuda.is_available() else "cpu"
    )

    # Generate text
    prompt = "The future of AI is"
    generated_text = eagle.generate(
        prompt=prompt,
        max_new_tokens=20,
        temperature=0.7,
        top_p=0.9
    )

    print(f"Prompt: {prompt}")
    print(f"Generated: {generated_text}")
